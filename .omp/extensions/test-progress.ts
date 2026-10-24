import { readdir, readFile } from "node:fs/promises";
import { join } from "node:path";
import type { ExtensionAPI } from "@oh-my-pi/pi-coding-agent";

interface RunningTest {
  name: string;
  started: number;
  progress: string | null;
}

interface TestProgressStatus {
  version: 1;
  build_dir: string;
  pid: number;
  started: number;
  updated: number;
  state: "running" | "finished";
  exit_code: number | null;
  done: number;
  total: number | null;
  failed: string[];
  running: RunningTest[];
}

interface TestRun {
  directory: string;
  status: TestProgressStatus;
}

const WIDGET_ID = "test-progress";

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function isNonNegativeInteger(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) && value >= 0;
}

function isRunningTest(value: unknown): value is RunningTest {
  return (
    isRecord(value) &&
    typeof value.name === "string" &&
    typeof value.started === "number" &&
    Number.isFinite(value.started) &&
    (typeof value.progress === "string" || value.progress === null)
  );
}

function isTestProgressStatus(value: unknown): value is TestProgressStatus {
  return (
    isRecord(value) &&
    value.version === 1 &&
    typeof value.build_dir === "string" &&
    typeof value.pid === "number" &&
    Number.isSafeInteger(value.pid) &&
    value.pid > 0 &&
    typeof value.started === "number" &&
    Number.isFinite(value.started) &&
    typeof value.updated === "number" &&
    Number.isFinite(value.updated) &&
    (value.state === "running" || value.state === "finished") &&
    (value.exit_code === null ||
      (typeof value.exit_code === "number" && Number.isInteger(value.exit_code))) &&
    isNonNegativeInteger(value.done) &&
    (value.total === null || isNonNegativeInteger(value.total)) &&
    Array.isArray(value.failed) &&
    value.failed.every((failure) => typeof failure === "string") &&
    Array.isArray(value.running) &&
    value.running.every(isRunningTest)
  );
}

async function readRun(path: string, directory: string): Promise<TestRun | undefined> {
  try {
    const contents = await readFile(path, "utf8");
    const value: unknown = JSON.parse(contents);
    if (!isTestProgressStatus(value)) return undefined;
    return { directory, status: value };
  } catch {
    return undefined;
  }
}

async function readRuns(cwd: string): Promise<TestRun[]> {
  const buildDirectory = join(cwd, "build");
  try {
    const entries = await readdir(buildDirectory, { withFileTypes: true });
    const directories = entries
      .filter((entry) => entry.isDirectory())
      .sort((left, right) => left.name.localeCompare(right.name));
    const runs = await Promise.all(
      directories.map((entry) =>
        readRun(join(buildDirectory, entry.name, "test_progress.json"), entry.name),
      ),
    );
    return runs.filter((run): run is TestRun => run !== undefined);
  } catch {
    return [];
  }
}

function isAliveOrRecent(pid: number, updated: number, now: number): boolean {
  try {
    process.kill(pid, 0);
    return true;
  } catch (error) {
    const code =
      isRecord(error) && typeof error.code === "string" ? error.code : undefined;
    if (code === "ESRCH") return false;
    if (code === "EPERM") return true;
    return now - updated < 30;
  }
}

function treeName(directory: string): string {
  if (directory === "release") return "Release";
  if (directory === "asan") return "ASan";
  return directory;
}

function renderRun(run: TestRun, now: number): string | undefined {
  const { directory, status } = run;
  if (status.state === "running") {
    if (!isAliveOrRecent(status.pid, status.updated, now)) return undefined;
    const total = status.total === null ? "?" : String(status.total);
    const details = status.running.slice(0, 2).map((test) =>
      test.progress ? `${test.name} ${test.progress}` : test.name,
    );
    const more = status.running.length - details.length;
    if (more > 0) details.push(`+${more} more`);
    return `${treeName(directory)} ${status.done}/${total} · ${status.failed.length} failed${details.length > 0 ? ` · ${details.join(" · ")}` : ""}`;
  }

  if (now - status.updated >= 60) return undefined;
  const failed = status.failed.length;
  const total = status.total ?? status.done;
  const passed = Math.max(0, status.done - failed);
  return `${treeName(directory)} finished: ${passed}/${total} passed, ${failed} failed`;
}

export default function testProgressExtension(pi: ExtensionAPI): void {
  pi.on("session_start", (_event, ctx) => {
    const refresh = async (): Promise<void> => {
      try {
        const now = Date.now() / 1000;
        const runs = await readRuns(ctx.cwd);
        const lines = runs
          .map((run) => renderRun(run, now))
          .filter((line): line is string => line !== undefined);
        ctx.ui.setWidget(
          WIDGET_ID,
          lines.length > 0 ? lines : undefined,
          { placement: "aboveEditor" },
        );
      } catch {
        // A malformed or inaccessible status file must not affect the session.
      }
    };

    try {
      const timer = ctx.setInterval(refresh, 2_000);
      void refresh();
      pi.on("session_shutdown", () => {
        try {
          ctx.clearTimer(timer);
          ctx.ui.setWidget(WIDGET_ID, undefined);
        } catch {
          // Shutdown cleanup is best-effort.
        }
      });
    } catch {
      // Extension callbacks must not take down the session.
    }
  });
}
