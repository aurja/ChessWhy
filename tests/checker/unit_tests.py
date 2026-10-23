from __future__ import annotations

import sys
import unittest
import unittest.mock
from pathlib import Path

import chess

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

import validate_report as checker

START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"


class CheckerRelationsTests(unittest.TestCase):
    def test_best_requires_search_line_and_equal_cp_or_mate_score(self) -> None:
        report: checker.JsonObject = {
            "candidates": [
                {
                    "eval": {"es": 0.5, "cp": 50, "mate": None},
                    "loss": 0.0,
                    "verdict": "best",
                    "in_multipv": True,
                    "allows_tactic": False,
                },
                {
                    "eval": {"es": 0.5, "cp": 49, "mate": None},
                    "loss": 0.0,
                    "verdict": "best",
                    "in_multipv": True,
                    "allows_tactic": False,
                },
            ],
            "asked_moves": [
                {
                    "eval": {"es": 0.5, "cp": 50, "mate": None},
                    "loss": 0.0,
                    "verdict": "best",
                    "in_multipv": False,
                    "allows_tactic": False,
                }
            ],
        }
        failures: list[checker.Failure] = []

        checker.check_score_relations(report, failures)

        self.assertIn(("/candidates/1/verdict", unittest.mock.ANY), failures)
        self.assertIn(("/asked_moves/0/verdict", unittest.mock.ANY), failures)
        self.assertTrue(
            checker.same_engine_score(
                {"cp": None, "mate": -3}, {"cp": None, "mate": -3}
            )
        )
        self.assertFalse(
            checker.same_engine_score(
                {"cp": None, "mate": -3}, {"cp": None, "mate": -2}
            )
        )

    def test_blunder_requires_allows_tactic(self) -> None:
        report: checker.JsonObject = {
            "candidates": [
                {
                    "eval": {"es": 0.6, "cp": 100, "mate": None},
                    "loss": 0.0,
                    "verdict": "best",
                    "in_multipv": True,
                    "allows_tactic": False,
                }
            ],
            "asked_moves": [
                {
                    "eval": {"es": 0.3, "cp": -200, "mate": None},
                    "loss": 0.3,
                    "verdict": "blunder",
                    "in_multipv": False,
                    "allows_tactic": False,
                }
            ],
        }
        failures: list[checker.Failure] = []

        checker.check_score_relations(report, failures)

        self.assertTrue(any(path == "/asked_moves/0/verdict" for path, _ in failures))

    def test_tactic_and_allowed_tactic_follow_consequence(self) -> None:
        move: checker.JsonObject = {
            "consequence": {
                "class": "material",
                "material": -1,
            },
            "eval": {"mate": None},
            "is_tactic": False,
            "allows_tactic": False,
            "detailed": True,
            "opportunity": None,
            "motifs": [],
            "root_motifs": [],
            "tactic_labels": [],
            "creates_threat": None,
            "refutation": None,
        }
        failures: list[checker.Failure] = []

        checker.check_move_annotations(move, "/candidate", failures)

        self.assertTrue(any(path == "/candidate/is_tactic" for path, _ in failures))
        self.assertTrue(any(path == "/candidate/allows_tactic" for path, _ in failures))

    def test_non_detailed_line_clears_parry_and_refutation_opportunity(self) -> None:
        move: checker.JsonObject = {
            "consequence": {"class": "positional", "material": 0},
            "eval": {"mate": None},
            "is_tactic": False,
            "allows_tactic": False,
            "detailed": False,
            "parries_threat": {"value": True},
            "creates_threat": None,
            "creates_threat_skipped": None,
            "opportunity": None,
            "motifs": [],
            "root_motifs": [],
            "tactic_labels": [],
            "refutation": {
                "line": [],
                "is_tactic": True,
                "opportunity": {},
                "motifs": [],
                "tactic_labels": [],
            },
        }
        failures: list[checker.Failure] = []

        checker.check_move_annotations(move, "/candidate", failures)

        paths = [path for path, _ in failures]
        self.assertIn("/candidate/parries_threat", paths)
        self.assertIn("/candidate/refutation/is_tactic", paths)
        self.assertIn("/candidate/refutation/opportunity", paths)

    def test_threat_skip_from_board_only_on_detailed_lines(self) -> None:
        root = chess.Board("6k1/5ppp/8/8/2B5/1q6/5PPP/4R1K1 w - - 0 1")
        check: checker.JsonObject = {
            "san": "Bxf7+",
            "uci": "c4f7",
            "detailed": False,
            "creates_threat_skipped": None,
        }
        failures: list[checker.Failure] = []

        checker.check_threat_skip(check, "/candidates/0", root, failures)
        self.assertEqual(failures, [])

        check["detailed"] = True
        checker.check_threat_skip(check, "/candidates/0", root, failures)
        self.assertEqual(
            [path for path, _ in failures], ["/candidates/0/creates_threat_skipped"]
        )

    def test_labels_and_contributions_are_only_on_tactic_lines(self) -> None:
        move: checker.JsonObject = {
            "consequence": {
                "class": "positional",
                "material": 0,
            },
            "eval": {"mate": None},
            "is_tactic": False,
            "allows_tactic": False,
            "detailed": True,
            "opportunity": None,
            "motifs": [{"contribution": "direct"}],
            "root_motifs": [],
            "tactic_labels": [{"tag": "removal_of_defender"}],
            "creates_threat": None,
            "refutation": None,
        }
        failures: list[checker.Failure] = []

        checker.check_move_annotations(move, "/candidate", failures)

        self.assertTrue(
            any(path == "/candidate/motifs/0/contribution" for path, _ in failures)
        )
        self.assertTrue(any(path == "/candidate/tactic_labels" for path, _ in failures))

    def test_opportunity_margin_threshold_and_forced_line(self) -> None:
        failures: list[checker.Failure] = []
        opportunity: checker.JsonObject = {
            "source": "engine",
            "forced": False,
            "alternative": {"san": "d5", "uci": "d7d5", "eval": {"es": 0.3}},
            "margin": 0.13,
            "is_opportunity": False,
        }

        _ = checker.check_opportunity_judgement(
            opportunity, "/opportunity", False, 260, None, None, failures
        )

        self.assertIn("/opportunity/is_opportunity", [path for path, _ in failures])
        forced_failures: list[checker.Failure] = []
        forced: checker.JsonObject = {
            "source": "rule",
            "forced": True,
            "alternative": None,
            "margin": None,
            "is_opportunity": False,
        }
        _ = checker.check_opportunity_judgement(
            forced, "/forced", True, None, None, None, forced_failures
        )
        self.assertEqual(forced_failures, [])

    def test_selection_deduplicates_and_keeps_typed_move_order(self) -> None:
        report: checker.JsonObject = {
            "input": {
                "fen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                "moves": "Nf3,e4,g1f3",
                "selection": {"keywords": [], "top": None, "moves": ["g1f3", "e2e4"]},
            },
            "position": {
                "fen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
            },
            "candidates": [{"uci": "e2e4", "in_multipv": True}],
            "asked_moves": [{"uci": "g1f3", "in_multipv": False}],
        }
        failures: list[checker.Failure] = []
        root = chess.Board(START_FEN)

        checker.check_selection_relations(report, root, failures)

        self.assertEqual(failures, [])
        input_info = report["input"]
        assert isinstance(input_info, dict)
        selection = input_info["selection"]
        assert isinstance(selection, dict)
        selection["moves"] = ["e2e4", "g1f3"]
        failures.clear()
        checker.check_selection_relations(report, root, failures)
        self.assertTrue(any(path == "/input/selection/moves" for path, _ in failures))

    def test_playable_truncated_requires_all_returned_lines_playable(self) -> None:
        report: checker.JsonObject = {
            "engine": {"multipv": 2},
            "position": {"legal_moves": 20},
            "candidates": [{"loss": 0.0}, {"loss": 0.05}],
            "playable_truncated": True,
        }
        failures: list[checker.Failure] = []

        checker.check_playable_truncated(report, failures)

        self.assertEqual(failures, [])
        candidates = report["candidates"]
        assert isinstance(candidates, list)
        second = candidates[1]
        assert isinstance(second, dict)
        second["loss"] = 0.051
        failures.clear()
        checker.check_playable_truncated(report, failures)
        self.assertEqual([path for path, _ in failures], ["/playable_truncated"])

    def test_projection_filters_motifs_and_unheld_or_repeated_changes(self) -> None:
        motifs: list[checker.JsonValue] = [
            {"contribution": "direct"},
            {"contribution": "indirect"},
            {"contribution": "none"},
            {"contribution": None},
        ]
        self.assertEqual(checker.contribution_filtered(motifs), motifs[:2])
        first: checker.JsonObject = {
            "tag": "bishop_pair",
            "side": "white",
            "pieces": [],
            "file": None,
            "value": None,
            "holds_at_end": True,
            "ply": 1,
        }
        other: checker.JsonObject = {
            **first,
            "tag": "rook_open_file",
            "ply": 2,
        }
        latest: checker.JsonObject = {**first, "ply": 3, "change": "disappears"}
        not_held: checker.JsonObject = {**first, "file": "a", "holds_at_end": False}

        self.assertEqual(
            checker.net_positional_changes([first, other, latest, not_held]),
            [other, latest],
        )
        connected_before: checker.JsonObject = {
            "tag": "connected_passed_pawns",
            "side": "white",
            "pieces": [
                {"square": "a6", "piece": "P"},
                {"square": "b6", "piece": "P"},
            ],
        }
        connected_after: checker.JsonObject = {
            **connected_before,
            "pieces": [
                {"square": "a7", "piece": "P"},
                {"square": "b7", "piece": "P"},
            ],
        }
        self.assertEqual(
            checker.positional_identity(connected_before),
            checker.positional_identity(connected_after),
        )
        chain_before: checker.JsonObject = {
            **connected_before,
            "tag": "pawn_chain",
        }
        chain_after: checker.JsonObject = {
            **connected_after,
            "tag": "pawn_chain",
        }
        self.assertNotEqual(
            checker.positional_identity(chain_before),
            checker.positional_identity(chain_after),
        )

    def test_projection_detects_move_fact_mismatch(self) -> None:
        assessment: checker.JsonObject = {
            "san": "e4",
            "uci": "e2e4",
            "eval": {"source": "engine", "es": 0.5, "cp": 0, "mate": None},
            "depth": 12,
            "loss": 0.0,
            "verdict": "best",
            "gives_check": False,
            "line": ["e4"],
            "walked_plies": 1,
            "consequence": {
                "source": "rule",
                "class": "positional",
                "mate_in": None,
                "material": 0,
                "promotes": False,
                "plies": 1,
            },
            "is_tactic": False,
            "allows_tactic": False,
            "opportunity": None,
            "motifs": [],
            "root_motifs": [],
            "tactic_labels": [],
            "positional": [],
            "parries_threat": None,
            "creates_threat": None,
            "refutation": None,
        }
        full: checker.JsonObject = {
            "schema_version": 6,
            "document": "full",
            "input": {
                "source": "fen",
                "fen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                "pgn_game": None,
                "pgn_ply": None,
                "moves": None,
                "selection": {"keywords": ["best"], "top": None, "moves": []},
                "level": "club",
                "budget": "standard",
            },
            "engine": {"multipv": 1},
            "position": {
                "fen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                "side_to_move": "white",
                "in_check": False,
                "legal_moves": 20,
                "terminal": None,
                "eval_white": assessment["eval"],
                "motifs": [],
                "positional": [],
            },
            "threats": {"white": None, "black": None},
            "candidates": [assessment],
            "asked_moves": [],
            "playable_truncated": False,
            "opening": {"source": "book", "current": None, "last_named": None},
            "games": {"source": "games", "by_source": []},
        }
        moves = checker.projected_move_document(full)
        failures: list[checker.Failure] = []

        checker.check_projection(full, moves, failures)

        self.assertEqual(failures, [])
        move_list = moves["moves"]
        assert isinstance(move_list, list)
        first_move = move_list[0]
        assert isinstance(first_move, dict)
        first_move["loss"] = 0.1
        failures.clear()
        checker.check_projection(full, moves, failures)
        self.assertTrue(any(path == "/moves/0/loss" for path, _ in failures))


if __name__ == "__main__":
    _ = unittest.main()
