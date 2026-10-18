#include "chesswhy/engine.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace chesswhy::engine {
namespace {

using Clock = std::chrono::steady_clock;
using Deadline = Clock::time_point;

std::string system_error_message(std::string_view operation, int error) {
    std::string message(operation);
    message += ": ";
    message += std::error_code(error, std::generic_category()).message();
    return message;
}

[[noreturn]] void throw_system_error(std::string_view operation) {
    throw EngineError(system_error_message(operation, errno));
}

void close_fd(int& fd) noexcept {
    if (fd >= 0) {
        (void)::close(fd);
        fd = -1;
    }
}

void close_pair(std::array<int, 2>& fds) noexcept {
    close_fd(fds[0]);
    close_fd(fds[1]);
}

void make_pipe(std::array<int, 2>& fds) {
    if (::pipe(fds.data()) < 0) {
        throw_system_error("engine pipe");
    }
}

void make_nonblocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw_system_error("engine pipe configuration");
    }
}

int timeout_milliseconds(Deadline deadline, std::string_view context) {
    const auto now = Clock::now();
    if (now >= deadline) {
        throw EngineError(std::string(context) + " timed out");
    }
    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - now).count();
    return static_cast<int>(remaining);
}

void wait_for_fd(int fd, short events, Deadline deadline, std::string_view context) {
    for (;;) {
        pollfd descriptor{.fd = fd, .events = events, .revents = 0};
        const int timeout = timeout_milliseconds(deadline, context);
        const int ready = ::poll(&descriptor, 1, timeout);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw_system_error(context);
        }
        if (ready == 0) {
            throw EngineError(std::string(context) + " timed out");
        }
        if ((descriptor.revents & POLLNVAL) != 0) {
            throw EngineError(std::string(context) + ": invalid engine pipe");
        }
        if ((descriptor.revents & (events | POLLERR | POLLHUP)) != 0) {
            return;
        }
    }
}

class SigpipeBlock final {
public:
    SigpipeBlock() {
        ::sigemptyset(&blocked_);
        ::sigaddset(&blocked_, SIGPIPE);
        const int result = ::pthread_sigmask(SIG_BLOCK, &blocked_, &previous_);
        if (result != 0) {
            throw EngineError(system_error_message("engine SIGPIPE handling", result));
        }
        active_ = true;

        sigset_t pending{};
        if (::sigpending(&pending) == 0) {
            was_pending_ = ::sigismember(&pending, SIGPIPE) == 1;
        }
    }

    ~SigpipeBlock() {
        if (active_) {
            (void)::pthread_sigmask(SIG_SETMASK, &previous_, nullptr);
        }
    }

    SigpipeBlock(const SigpipeBlock&) = delete;
    SigpipeBlock& operator=(const SigpipeBlock&) = delete;

    void consume_new_signal() noexcept {
        if (was_pending_) {
            return;
        }
        const timespec timeout{};
        (void)::sigtimedwait(&blocked_, nullptr, &timeout);
    }

private:
    sigset_t blocked_{};
    sigset_t previous_{};
    bool active_ = false;
    bool was_pending_ = false;
};

void write_all(int fd, std::string_view text, Deadline deadline, std::string_view context) {
    SigpipeBlock sigpipe_block;
    std::size_t written = 0;
    while (written < text.size()) {
        const auto result = ::write(fd, text.data() + written, text.size() - written);
        if (result > 0) {
            written += static_cast<std::size_t>(result);
            continue;
        }
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            wait_for_fd(fd, POLLOUT, deadline, context);
            continue;
        }
        if (result < 0 && errno == EPIPE) {
            sigpipe_block.consume_new_signal();
        }
        throw_system_error(context);
    }
}

std::string_view next_token(std::string_view text, std::size_t& offset) {
    while (offset < text.size() && (text[offset] == ' ' || text[offset] == '\t' ||
                                    text[offset] == '\r' || text[offset] == '\n')) {
        ++offset;
    }
    const auto start = offset;
    while (offset < text.size() && text[offset] != ' ' && text[offset] != '\t' &&
           text[offset] != '\r' && text[offset] != '\n') {
        ++offset;
    }
    return text.substr(start, offset - start);
}

template <typename Integer>
bool parse_integer(std::string_view token, Integer& value) {
    if (token.empty()) {
        return false;
    }
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    return result.ec == std::errc{} && result.ptr == token.data() + token.size();
}

std::optional<std::uint64_t> info_nodes(std::string_view text) {
    std::size_t offset = 0;
    if (next_token(text, offset) != "info") {
        return std::nullopt;
    }
    while (offset < text.size()) {
        const auto token = next_token(text, offset);
        if (token.empty() || token == "pv" || token == "string") {
            break;
        }
        if (token == "nodes") {
            std::uint64_t nodes = 0;
            if (!parse_integer(next_token(text, offset), nodes)) {
                return std::nullopt;
            }
            return nodes;
        }
    }
    return std::nullopt;
}

void child_report_error(int status_fd, int error) noexcept {
    const char* data = reinterpret_cast<const char*>(&error);
    std::size_t written = 0;
    while (written < sizeof(error)) {
        const auto result = ::write(status_fd, data + written, sizeof(error) - written);
        if (result > 0) {
            written += static_cast<std::size_t>(result);
        } else if (result < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    ::_exit(127);
}

} // namespace

struct Engine::Impl {
    pid_t pid = -1;
    int input_fd = -1;
    int output_fd = -1;
    std::string binary_path;
    std::string engine_name;
    std::string buffered_output;
    std::size_t buffered_offset = 0;

    ~Impl() { stop(); }

    void start(const Options& options) {
        binary_path = options.binary.string();
        struct stat status{};
        if (binary_path.empty() || ::stat(binary_path.c_str(), &status) < 0 ||
            !S_ISREG(status.st_mode) || ::access(binary_path.c_str(), X_OK) < 0) {
            throw EngineError("engine binary is missing or not executable: " + binary_path);
        }

        std::array<int, 2> child_input{-1, -1};
        std::array<int, 2> child_output{-1, -1};
        std::array<int, 2> exec_status{-1, -1};
        try {
            make_pipe(child_input);
            make_pipe(child_output);
            make_pipe(exec_status);
            const int descriptor_flags = ::fcntl(exec_status[1], F_GETFD);
            if (descriptor_flags < 0 ||
                ::fcntl(exec_status[1], F_SETFD, descriptor_flags | FD_CLOEXEC) < 0) {
                throw_system_error("engine exec status pipe configuration");
            }
        } catch (...) {
            close_pair(child_input);
            close_pair(child_output);
            close_pair(exec_status);
            throw;
        }

        const pid_t child = ::fork();
        if (child < 0) {
            const int error = errno;
            close_pair(child_input);
            close_pair(child_output);
            close_pair(exec_status);
            throw EngineError(system_error_message("engine fork", error));
        }
        if (child == 0) {
            if (::dup2(child_input[0], STDIN_FILENO) < 0 ||
                ::dup2(child_output[1], STDOUT_FILENO) < 0) {
                child_report_error(exec_status[1], errno);
            }
            const int null_fd = ::open("/dev/null", O_WRONLY);
            if (null_fd < 0 || ::dup2(null_fd, STDERR_FILENO) < 0) {
                child_report_error(exec_status[1], errno);
            }
            if (null_fd > STDERR_FILENO) {
                (void)::close(null_fd);
            }
            const std::array<int, 5> descriptors{child_input[0], child_input[1], child_output[0],
                                                 child_output[1], exec_status[0]};
            for (const int descriptor : descriptors) {
                if (descriptor > STDERR_FILENO && descriptor != exec_status[1]) {
                    (void)::close(descriptor);
                }
            }
            std::array<char*, 2> arguments{const_cast<char*>(binary_path.c_str()), nullptr};
            ::execv(binary_path.c_str(), arguments.data());
            child_report_error(exec_status[1], errno);
        }

        pid = child;
        (void)::close(child_input[0]);
        child_input[0] = -1;
        (void)::close(child_output[1]);
        child_output[1] = -1;
        (void)::close(exec_status[1]);
        exec_status[1] = -1;
        input_fd = child_input[1];
        child_input[1] = -1;
        output_fd = child_output[0];
        child_output[0] = -1;

        try {
            make_nonblocking(input_fd);
            make_nonblocking(output_fd);
            make_nonblocking(exec_status[0]);
            const auto startup_deadline = Clock::now() + std::chrono::seconds(10);
            wait_for_exec(exec_status[0], startup_deadline);
            close_fd(exec_status[0]);

            write_command("uci", startup_deadline, "engine startup");
            bool received_uciok = false;
            while (!received_uciok) {
                auto line = read_line(startup_deadline, "engine startup");
                if (line.starts_with("id name ")) {
                    engine_name = line.substr(8);
                } else if (line == "uciok") {
                    received_uciok = true;
                }
            }
            if (engine_name.empty()) {
                throw EngineError("engine startup returned no id name: " + binary_path);
            }

            write_command("setoption name Threads value " + std::to_string(options.threads),
                          startup_deadline, "engine startup");
            write_command("setoption name Hash value " + std::to_string(options.hash_mb),
                          startup_deadline, "engine startup");
            write_command("setoption name UCI_ShowWDL value true", startup_deadline,
                          "engine startup");
            ready(startup_deadline, "engine startup");
        } catch (...) {
            close_fd(exec_status[0]);
            throw;
        }
        close_fd(exec_status[0]);
    }

    void wait_for_exec(int status_fd, Deadline deadline) {
        std::array<char, sizeof(int)> error_bytes{};
        std::size_t received = 0;
        for (;;) {
            wait_for_fd(status_fd, POLLIN, deadline, "engine startup");
            const auto count =
                ::read(status_fd, error_bytes.data() + received, error_bytes.size() - received);
            if (count > 0) {
                received += static_cast<std::size_t>(count);
                if (received == error_bytes.size()) {
                    int error = 0;
                    std::memcpy(&error, error_bytes.data(), sizeof(error));
                    int status = 0;
                    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
                    }
                    pid = -1;
                    throw EngineError(
                        system_error_message("engine exec failed for " + binary_path, error));
                }
                continue;
            }
            if (count == 0) {
                if (received != 0) {
                    throw EngineError("engine exec status was incomplete: " + binary_path);
                }
                return;
            }
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            throw_system_error("engine exec status");
        }
    }

    void write_command(std::string_view command, Deadline deadline,
                       std::string_view context) const {
        std::string line(command);
        line.push_back('\n');
        write_all(input_fd, line, deadline, context);
    }

    std::string read_line(Deadline deadline, std::string_view context) {
        for (;;) {
            const auto newline = buffered_output.find('\n', buffered_offset);
            if (newline != std::string::npos) {
                std::string line =
                    buffered_output.substr(buffered_offset, newline - buffered_offset);
                buffered_offset = newline + 1;
                if (buffered_offset == buffered_output.size()) {
                    buffered_output.clear();
                    buffered_offset = 0;
                } else if (buffered_offset > 4096) {
                    buffered_output.erase(0, buffered_offset);
                    buffered_offset = 0;
                }
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                return line;
            }

            wait_for_fd(output_fd, POLLIN, deadline, context);
            std::array<char, 4096> chunk{};
            const auto count = ::read(output_fd, chunk.data(), chunk.size());
            if (count > 0) {
                buffered_output.append(chunk.data(), static_cast<std::size_t>(count));
                continue;
            }
            if (count == 0) {
                throw EngineError("engine exited while reading output");
            }
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            throw_system_error(context);
        }
    }

    void ready(Deadline deadline, std::string_view context) {
        write_command("isready", deadline, context);
        for (;;) {
            if (read_line(deadline, context) == "readyok") {
                return;
            }
        }
    }

    SearchResult search(const SearchRequest& request) {
        if (request.nodes == 0 || request.multipv < 1) {
            throw EngineError("engine search requires positive nodes and multipv");
        }
        const auto deadline = Clock::now() + std::chrono::seconds(120);
        write_command("ucinewgame", deadline, "engine search");
        write_command("setoption name MultiPV value " + std::to_string(request.multipv), deadline,
                      "engine search");
        ready(deadline, "engine search");
        write_command("position fen " + request.fen, deadline, "engine search");

        std::string go = "go nodes " + std::to_string(request.nodes);
        if (!request.searchmoves_uci.empty()) {
            go += " searchmoves";
            for (const auto& move : request.searchmoves_uci) {
                go.push_back(' ');
                go += move;
            }
        }
        write_command(go, deadline, "engine search");

        SearchResult result;
        MultiPvCollector collector;
        bool got_bestmove = false;
        while (!got_bestmove) {
            const auto text = read_line(deadline, "engine search");
            std::size_t offset = 0;
            const auto first = next_token(text, offset);
            if (first == "info") {
                result.nodes = info_nodes(text).value_or(0);
                collector.add(text);
            } else if (first == "bestmove") {
                const auto move = next_token(text, offset);
                if (move.empty()) {
                    throw EngineError("engine returned malformed bestmove");
                }
                result.bestmove_uci = std::string(move);
                got_bestmove = true;
            }
        }

        result.lines = std::move(collector).finish();
        return result;
    }

    void write_quit() noexcept {
        if (input_fd < 0) {
            return;
        }

        sigset_t blocked{};
        sigset_t previous{};
        (void)::sigemptyset(&blocked);
        (void)::sigaddset(&blocked, SIGPIPE);
        if (::pthread_sigmask(SIG_BLOCK, &blocked, &previous) != 0) {
            return;
        }

        sigset_t pending{};
        const bool was_pending =
            ::sigpending(&pending) == 0 && ::sigismember(&pending, SIGPIPE) == 1;
        constexpr std::string_view command = "quit\n";
        std::size_t written = 0;
        const auto deadline = Clock::now() + std::chrono::milliseconds(200);
        while (written < command.size()) {
            const auto count =
                ::write(input_fd, command.data() + written, command.size() - written);
            if (count > 0) {
                written += static_cast<std::size_t>(count);
                continue;
            }
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                const auto remaining = deadline - Clock::now();
                if (remaining <= Clock::duration::zero()) {
                    break;
                }
                pollfd descriptor{.fd = input_fd, .events = POLLOUT, .revents = 0};
                const auto timeout =
                    std::chrono::ceil<std::chrono::milliseconds>(remaining).count();
                const int ready = ::poll(&descriptor, 1, static_cast<int>(timeout));
                if (ready < 0 && errno == EINTR) {
                    continue;
                }
                if (ready > 0) {
                    continue;
                }
                break;
            }
            if (count < 0 && errno == EPIPE && !was_pending) {
                const timespec no_wait{};
                (void)::sigtimedwait(&blocked, nullptr, &no_wait);
            }
            break;
        }
        (void)::pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    }

    void stop() noexcept {
        if (pid > 0) {
            write_quit();
            close_fd(input_fd);

            const auto deadline = Clock::now() + std::chrono::seconds(2);
            int status = 0;
            while (Clock::now() < deadline) {
                const pid_t result = ::waitpid(pid, &status, WNOHANG);
                if (result == pid || (result < 0 && errno == ECHILD)) {
                    pid = -1;
                    break;
                }
                if (result < 0 && errno != EINTR) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (pid > 0) {
                (void)::kill(pid, SIGKILL);
                while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
                }
                pid = -1;
            }
        }
        close_fd(input_fd);
        close_fd(output_fd);
    }
};

Engine::Engine(const Options& options) : impl_(std::make_unique<Impl>()) {
    impl_->start(options);
}

Engine::~Engine() = default;
Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;

const std::string& Engine::name() const {
    if (!impl_) {
        throw EngineError("engine is not running");
    }
    return impl_->engine_name;
}

SearchResult Engine::search(const SearchRequest& request) {
    if (!impl_) {
        throw EngineError("engine is not running");
    }
    return impl_->search(request);
}

} // namespace chesswhy::engine
