from __future__ import annotations

import sys
import unittest
import unittest.mock
from collections.abc import Callable
from copy import deepcopy
from pathlib import Path

import chess
from jsonschema import Draft202012Validator

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

import validate_report as checker

Mutation = Callable[[checker.JsonObject], None]


def object_value(value: checker.JsonValue) -> checker.JsonObject:
    result = checker.as_object(value)
    assert result is not None
    return result


def object_field(value: checker.JsonValue, key: str) -> checker.JsonObject:
    return object_value(object_value(value).get(key))


def array_field(value: checker.JsonValue, key: str) -> list[checker.JsonValue]:
    result = checker.as_array(object_value(value).get(key))
    assert result is not None
    return result


def first_candidate(report: checker.JsonValue) -> checker.JsonObject:
    return object_value(array_field(report, "candidates")[0])


START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"


def forced_test_for_e5() -> checker.JsonObject:
    return {
        "legal_moves": 20,
        "lines": [
            {"san": "e5", "es": 0.5, "acceptable": True, "stops_threat": True},
            {"san": "c5", "es": 0.49, "acceptable": True, "stops_threat": True},
            {"san": "e6", "es": 0.48, "acceptable": True, "stops_threat": True},
            {"san": "d5", "es": 0.47, "acceptable": True, "stops_threat": True},
            {"san": "Nf6", "es": 0.44, "acceptable": False, "stops_threat": None},
        ],
        "forced": True,
    }


def tempo_semantic_report() -> checker.JsonObject:
    return {
        "position": {
            "fen": START_FEN,
            "in_check": False,
            "terminal": None,
            "tempo_value": {
                "side": "white",
                "moves": 0.5,
                "passes": 0.4,
                "value": 0.1,
                "label": None,
                "source": "engine",
            },
            "positional": [],
        },
        "threats": {
            "black": {},
            "null_move_reliable": {"value": True},
        },
        "candidates": [
            {
                "san": "e4",
                "eval": {"es": 0.5},
                "detailed": True,
                "line": ["e4", "e5"],
                "walked_plies": 2,
                "consequence": {
                    "source": "rule",
                    "class": "positional",
                    "mate_in": None,
                    "material": 0,
                    "promotes": False,
                    "plies": 1,
                    "terminal_draw": None,
                    "settled": True,
                },
                "creates_threat_skipped": None,
                "creates_threat": {"consequence": {"class": "material", "material": 1}},
                "tempo_value": {
                    "side": "black",
                    "moves": 0.5,
                    "passes": 0.48,
                    "value": 0.02,
                    "label": "non_tempo",
                    "source": "engine",
                },
                "tempo_ledger_skipped": None,
                "tempo_ledger": {
                    "plies": [
                        {
                            "ply": 1,
                            "san": "e4",
                            "side": "white",
                            "threatening": True,
                            "by_check": False,
                            "threat_rank": 2,
                            "stops_threat": None,
                            "label": "threatening",
                            "tempo_gained": True,
                            "reply_gain": None,
                            "forced_test": None,
                        },
                        {
                            "ply": 2,
                            "san": "e5",
                            "side": "black",
                            "threatening": False,
                            "by_check": False,
                            "threat_rank": 0,
                            "stops_threat": True,
                            "label": "pure_reply",
                            "tempo_gained": False,
                            "reply_gain": {
                                "material": 0,
                                "mates": False,
                                "through_ply": 2,
                                "settled": True,
                                "gains": False,
                            },
                            "forced_test": forced_test_for_e5(),
                        },
                    ],
                    "tempi": {"white": 1, "black": 0},
                    "runs": [
                        {
                            "holder": "white",
                            "start_ply": 1,
                            "end_ply": 2,
                            "tempi": 1,
                            "ending": "unfinished",
                            "outcome": "unknown",
                            "source": "rule",
                        }
                    ],
                    "source": "engine",
                },
                "positional": [],
            }
        ],
        "asked_moves": [],
    }


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

    def test_projection_filters_motifs_and_orders_kept_net_changes(self) -> None:
        motifs: list[checker.JsonValue] = [
            {"contribution": "direct"},
            {"contribution": "indirect"},
            {"contribution": "none"},
            {"contribution": None},
        ]
        filtered = checker.relevant_motifs(motifs)
        self.assertEqual(
            filtered,
            [
                {"contribution": "direct", "also": []},
                {"contribution": "indirect", "also": []},
            ],
        )
        self.assertNotIn("also", object_value(motifs[0]))
        first: checker.JsonObject = {
            "tag": "bishop_pair",
            "side": "white",
            "pieces": [],
            "file": None,
            "value": None,
            "change": "appears",
            "net_change": False,
            "in_use": None,
            "holds_at_end": True,
            "ply": 1,
        }
        other: checker.JsonObject = {
            **first,
            "tag": "rook_open_file",
            "net_change": True,
            "in_use": True,
            "ply": 2,
        }
        latest: checker.JsonObject = {
            **first,
            "change": "disappears",
            "net_change": True,
            "in_use": True,
            "ply": 3,
        }
        not_held: checker.JsonObject = {
            **first,
            "file": "a",
            "holds_at_end": False,
        }

        self.assertEqual(
            checker.net_positional_changes([first, other, latest, not_held]),
            [latest, other],
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

    def test_projection_merges_restricted_motif_repeats(self) -> None:
        shared: checker.JsonObject = {
            "side": "black",
            "ply": 3,
            "actor": "b a7",
            "targets": ["B c5", "R f2"],
            "restriction": "along diagonal a7-g1",
            "contribution": "direct",
        }

        def motif(tag: str, threat: checker.JsonValue) -> checker.JsonObject:
            return {"tag": tag, **shared, "threat": threat}

        relative_pin = motif("pin_relative", None)
        first_tactical_pin = motif("pin_tactical", {"move": "Ba7-f2"})
        second_tactical_pin = motif("pin_tactical", {"move": "Ba7-c5"})

        def full_report(motifs: list[checker.JsonValue]) -> checker.JsonObject:
            return {
                "schema_version": 9,
                "document": "full",
                "input": {"selection": {"keywords": ["all"], "top": None, "moves": []}},
                "engine": {"multipv": 1},
                "position": {
                    "fen": START_FEN,
                    "side_to_move": "white",
                    "terminal": None,
                },
                "candidates": [
                    {
                        "uci": "e2e4",
                        "verdict": "best",
                        "line": ["e4"],
                        "walked_plies": 1,
                        "motifs": motifs,
                    }
                ],
                "asked_moves": [],
                "playable_truncated": False,
                "threats": {},
                "games": {"by_source": []},
            }

        report = full_report([relative_pin, first_tactical_pin])
        moves = checker.projected_move_document(report)
        failures: list[checker.Failure] = []
        checker.check_projection(report, moves, failures)
        self.assertEqual(failures, [])
        projected_move = object_value(array_field(moves, "moves")[0])
        projected_motifs = array_field(projected_move, "motifs")
        self.assertEqual(
            projected_motifs,
            [
                {
                    **relative_pin,
                    "also": [
                        {
                            "tag": "pin_tactical",
                            "threat": first_tactical_pin["threat"],
                        }
                    ],
                }
            ],
        )
        self.assertNotIn("also", relative_pin)
        self.assertNotIn("also", first_tactical_pin)

        unmerged = deepcopy(moves)
        unmerged_move = object_value(array_field(unmerged, "moves")[0])
        unmerged_move["motifs"] = [
            deepcopy(relative_pin),
            deepcopy(first_tactical_pin),
        ]
        unmerged_failures: list[checker.Failure] = []
        checker.check_projection(report, unmerged, unmerged_failures)
        self.assertTrue(
            any(path.startswith("/moves/0/motifs") for path, _ in unmerged_failures)
        )

        three_report = full_report(
            [relative_pin, first_tactical_pin, second_tactical_pin]
        )
        three_moves = checker.projected_move_document(three_report)
        three_failures: list[checker.Failure] = []
        checker.check_projection(three_report, three_moves, three_failures)
        self.assertEqual(three_failures, [])

        dropped = deepcopy(three_moves)
        dropped_motifs = array_field(
            object_value(array_field(dropped, "moves")[0]), "motifs"
        )
        dropped_also = array_field(object_value(dropped_motifs[0]), "also")
        _ = dropped_also.pop()
        dropped_failures: list[checker.Failure] = []
        checker.check_projection(three_report, dropped, dropped_failures)
        self.assertTrue(
            any(
                path.startswith("/moves/0/motifs/0/also")
                for path, _ in dropped_failures
            )
        )

        wrong_order = deepcopy(three_moves)
        wrong_order_motifs = array_field(
            object_value(array_field(wrong_order, "moves")[0]), "motifs"
        )
        wrong_order_also = array_field(object_value(wrong_order_motifs[0]), "also")
        wrong_order_also.reverse()
        order_failures: list[checker.Failure] = []
        checker.check_projection(three_report, wrong_order, order_failures)
        self.assertTrue(
            any(path.startswith("/moves/0/motifs/0/also") for path, _ in order_failures)
        )

    def test_moves_schema_ties_also_threat_to_tag(self) -> None:
        schema_path = (
            Path(__file__).resolve().parents[2] / "schema" / "moves.v9.schema.json"
        )
        schema = object_value(checker.read_json(schema_path))
        defs = object_field(schema, "$defs")
        items = object_field(
            object_field(
                object_field(object_field(defs, "motif"), "properties"), "also"
            ),
            "items",
        )
        validator = Draft202012Validator({**items, "$defs": defs})
        threat: checker.JsonObject = {"kind": "capture", "square": "f2"}
        cases: list[tuple[str, checker.JsonValue, bool]] = [
            ("pin_tactical", threat, True),
            ("pin_tactical", None, False),
            ("pin_relative", None, True),
            ("pin_relative", threat, False),
        ]
        for tag, label_threat, valid in cases:
            with self.subTest(tag=tag, threat=label_threat):
                label: checker.JsonObject = {"tag": tag, "threat": label_threat}
                self.assertEqual(not checker.validation_errors(validator, label), valid)

    def test_projection_detects_move_fact_mismatch(self) -> None:
        assessment: checker.JsonObject = {
            "san": "e4",
            "uci": "e2e4",
            "eval": {"source": "engine", "es": 0.5, "cp": 0, "mate": None},
            "depth": 12,
            "loss": 0.0,
            "verdict": "best",
            "gives_check": False,
            "line": ["e4", "e5"],
            "walked_plies": 2,
            "consequence": {
                "source": "rule",
                "class": "positional",
                "mate_in": None,
                "material": 0,
                "promotes": False,
                "plies": 1,
                "terminal_draw": None,
                "settled": True,
            },
            "is_tactic": False,
            "allows_tactic": False,
            "opportunity": None,
            "motifs": [],
            "root_motifs": [],
            "tactic_labels": [],
            "positional": [
                {
                    "source": "rule",
                    "ply": 1,
                    "change": "appears",
                    "holds_at_end": True,
                    "net_change": True,
                    "captured": False,
                    "in_use": True,
                    "tag": "bishop_pair",
                    "side": "white",
                    "pieces": [],
                    "file": None,
                    "value": None,
                },
                {
                    "source": "rule",
                    "ply": 1,
                    "change": "appears",
                    "holds_at_end": True,
                    "net_change": True,
                    "captured": False,
                    "in_use": True,
                    "tag": "mobility_lead",
                    "side": "white",
                    "pieces": [],
                    "file": None,
                    "value": 10,
                },
            ],
            "tempo_value": {
                "side": "black",
                "moves": 0.5,
                "passes": 0.48,
                "value": 0.02,
                "label": "non_tempo",
                "source": "engine",
            },
            "tempo_ledger": object_field(
                first_candidate(tempo_semantic_report()), "tempo_ledger"
            ),
            "tempo_ledger_skipped": None,
            "parries_threat": None,
            "creates_threat": None,
            "refutation": None,
            "refutation_skipped": None,
        }
        full: checker.JsonObject = {
            "schema_version": 9,
            "document": "full",
            "input": {
                "source": "fen",
                "fen": START_FEN,
                "pgn_game": None,
                "pgn_ply": None,
                "moves": None,
                "selection": {"keywords": ["best"], "top": None, "moves": []},
                "level": "club",
                "budget": "standard",
            },
            "engine": {"multipv": 1},
            "position": {
                "fen": START_FEN,
                "side_to_move": "white",
                "in_check": False,
                "legal_moves": 20,
                "terminal": None,
                "eval_white": assessment["eval"],
                "tempo_value": {
                    "side": "white",
                    "moves": 0.5,
                    "passes": 0.3995,
                    "value": 0.1005,
                    "label": "tempo_dependent",
                    "source": "engine",
                },
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
        self.assertEqual(
            first_move["impact_order"],
            ["tempo", "positional_dynamic", "positional_static"],
        )
        move_keys = list(first_move)
        self.assertLess(move_keys.index("positional"), move_keys.index("tempo_value"))
        self.assertLess(
            move_keys.index("impact_order"), move_keys.index("parried_threat")
        )
        position_keys = list(object_value(moves["position"]))
        self.assertLess(
            position_keys.index("eval_white"), position_keys.index("tempo_value")
        )
        self.assertEqual(len(array_field(first_move, "line_fens")), 2)

        def remove_last_line_fen(document: checker.JsonObject) -> None:
            _ = array_field(array_field(document, "moves")[0], "line_fens").pop()

        mutations: list[tuple[str, Mutation]] = [
            (
                "/position/tempo_value",
                lambda document: object_field(
                    object_field(document, "position"), "tempo_value"
                ).update({"passes": 0.4}),
            ),
            (
                "/moves/0/tempo_value",
                lambda document: object_field(
                    array_field(document, "moves")[0], "tempo_value"
                ).update({"value": 0.03}),
            ),
            (
                "/moves/0/tempo_gained",
                lambda document: object_value(
                    array_field(array_field(document, "moves")[0], "tempo_gained")[0]
                ).update({"side": "black"}),
            ),
            (
                "/moves/0/initiative",
                lambda document: object_value(
                    array_field(array_field(document, "moves")[0], "initiative")[0]
                ).update({"ending": "spent"}),
            ),
            (
                "/moves/0/initiative",
                lambda document: object_value(
                    array_field(array_field(document, "moves")[0], "initiative")[0]
                ).update({"outcome": "evaporated"}),
            ),
            (
                "/moves/0/consequence/terminal_draw",
                lambda document: object_field(
                    array_field(document, "moves")[0], "consequence"
                ).update({"terminal_draw": {"kind": "stalemate", "ply": 1}}),
            ),
            (
                "/moves/0/consequence/settled",
                lambda document: object_field(
                    array_field(document, "moves")[0], "consequence"
                ).update({"settled": False}),
            ),
            (
                "/moves/0/refutation_skipped",
                lambda document: object_value(array_field(document, "moves")[0]).update(
                    {"refutation_skipped": "game_over"}
                ),
            ),
            (
                "/moves/0/positional/0/captured",
                lambda document: object_value(
                    array_field(array_field(document, "moves")[0], "positional")[0]
                ).update({"captured": True}),
            ),
            (
                "/moves/0/impact_order",
                lambda document: array_field(
                    array_field(document, "moves")[0], "impact_order"
                ).__setitem__(0, "motifs"),
            ),
            (
                "/moves/0/positional",
                lambda document: object_value(
                    array_field(array_field(document, "moves")[0], "positional")[0]
                ).update({"tag": "outpost"}),
            ),
            (
                "/moves/0/line_fens/0",
                lambda document: array_field(
                    array_field(document, "moves")[0], "line_fens"
                ).__setitem__(0, START_FEN),
            ),
            (
                "/moves/0/line_fens",
                remove_last_line_fen,
            ),
        ]
        for pointer, mutate in mutations:
            with self.subTest(pointer=pointer):
                mutant = deepcopy(moves)
                mutate(mutant)
                mutant_failures: list[checker.Failure] = []
                checker.check_projection(full, mutant, mutant_failures)
                self.assertTrue(
                    any(path.startswith(pointer) for path, _ in mutant_failures)
                )

    def assert_semantic_mutation_fails(self, mutate: Mutation, pointer: str) -> None:
        report = deepcopy(tempo_semantic_report())
        mutate(report)
        failures: list[checker.Failure] = []
        checker.check_tempo_and_positional_semantics(
            report, chess.Board(START_FEN), failures
        )
        self.assertTrue(
            any(path.startswith(pointer) for path, _ in failures),
            msg=f"mutant did not fail {pointer}: {failures}",
        )

    def test_semantic_relations_reject_independent_mutants(self) -> None:
        report = tempo_semantic_report()
        failures: list[checker.Failure] = []
        checker.check_tempo_and_positional_semantics(
            report, chess.Board(START_FEN), failures
        )
        self.assertEqual(failures, [])

        mutations: list[tuple[str, Mutation]] = [
            (
                "/position/tempo_value/side",
                lambda value: object_field(
                    object_field(value, "position"), "tempo_value"
                ).update({"side": "black"}),
            ),
            (
                "/position/tempo_value/value",
                lambda value: object_field(
                    object_field(value, "position"), "tempo_value"
                ).update({"value": 0.05}),
            ),
            (
                "/position/tempo_value/label",
                lambda value: object_field(
                    object_field(value, "position"), "tempo_value"
                ).update({"label": "tempo_dependent"}),
            ),
            (
                "/candidates/0/tempo_value/moves",
                lambda value: object_field(
                    first_candidate(value), "tempo_value"
                ).update({"moves": 0.49}),
            ),
            (
                "/candidates/0/tempo_value/value",
                lambda value: object_field(
                    first_candidate(value), "tempo_value"
                ).update({"value": 0.03}),
            ),
            (
                "/candidates/0/tempo_value/label",
                lambda value: object_field(
                    first_candidate(value), "tempo_value"
                ).update({"label": "zugzwang"}),
            ),
            (
                "/candidates/0/tempo_value/side",
                lambda value: object_field(
                    first_candidate(value), "tempo_value"
                ).update({"side": "white"}),
            ),
            (
                "/candidates/0/tempo_ledger/plies/0/ply",
                lambda value: object_value(
                    array_field(
                        object_field(first_candidate(value), "tempo_ledger"),
                        "plies",
                    )[0]
                ).update({"ply": 2}),
            ),
            (
                "/candidates/0/tempo_ledger/plies/0/san",
                lambda value: object_value(
                    array_field(
                        object_field(first_candidate(value), "tempo_ledger"),
                        "plies",
                    )[0]
                ).update({"san": "d4"}),
            ),
            (
                "/candidates/0/tempo_ledger/plies/0/side",
                lambda value: object_value(
                    array_field(
                        object_field(first_candidate(value), "tempo_ledger"),
                        "plies",
                    )[0]
                ).update({"side": "black"}),
            ),
            (
                "/candidates/0/tempo_ledger/plies/0/by_check",
                lambda value: object_value(
                    array_field(
                        object_field(first_candidate(value), "tempo_ledger"),
                        "plies",
                    )[0]
                ).update({"by_check": True}),
            ),
            (
                "/candidates/0/tempo_ledger/plies/0/threat_rank",
                lambda value: object_value(
                    array_field(
                        object_field(first_candidate(value), "tempo_ledger"),
                        "plies",
                    )[0]
                ).update({"threat_rank": 1}),
            ),
            (
                "/candidates/0/tempo_ledger/plies/0/threatening",
                lambda value: object_value(
                    array_field(
                        object_field(first_candidate(value), "tempo_ledger"),
                        "plies",
                    )[0]
                ).update({"threatening": False}),
            ),
            (
                "/candidates/0/tempo_ledger/plies/1/stops_threat",
                lambda value: object_value(
                    array_field(
                        object_field(first_candidate(value), "tempo_ledger"),
                        "plies",
                    )[1]
                ).update({"stops_threat": None}),
            ),
            (
                "/candidates/0/tempo_ledger/plies/1/label",
                lambda value: object_value(
                    array_field(
                        object_field(first_candidate(value), "tempo_ledger"),
                        "plies",
                    )[1]
                ).update({"label": "neither"}),
            ),
            (
                "/candidates/0/tempo_ledger/plies/0/tempo_gained",
                lambda value: object_value(
                    array_field(
                        object_field(first_candidate(value), "tempo_ledger"),
                        "plies",
                    )[0]
                ).update({"tempo_gained": False}),
            ),
            (
                "/candidates/0/tempo_ledger/tempi/white",
                lambda value: object_field(
                    object_field(first_candidate(value), "tempo_ledger"),
                    "tempi",
                ).update({"white": 0}),
            ),
            (
                "/candidates/0/tempo_ledger/runs",
                lambda value: object_field(
                    first_candidate(value), "tempo_ledger"
                ).update({"runs": []}),
            ),
            (
                "/candidates/0/tempo_ledger/runs/0/end_ply",
                lambda value: object_value(
                    array_field(
                        object_field(first_candidate(value), "tempo_ledger"),
                        "runs",
                    )[0]
                ).update({"end_ply": 1}),
            ),
            (
                "/candidates/0/tempo_ledger/runs/0/ending",
                lambda value: object_value(
                    array_field(
                        object_field(first_candidate(value), "tempo_ledger"),
                        "runs",
                    )[0]
                ).update({"ending": "spent"}),
            ),
            (
                "/candidates/0/tempo_ledger/runs/0/outcome",
                lambda value: object_value(
                    array_field(
                        object_field(first_candidate(value), "tempo_ledger"),
                        "runs",
                    )[0]
                ).update({"outcome": "converted"}),
            ),
            (
                "/candidates/0/tempo_ledger_skipped",
                lambda value: first_candidate(value).update(
                    {"tempo_ledger_skipped": "not_detailed"}
                ),
            ),
            (
                "/candidates/0/tempo_ledger",
                lambda value: first_candidate(value).update(
                    {"tempo_ledger": None, "tempo_ledger_skipped": "not_detailed"}
                ),
            ),
            (
                "/candidates/0/tempo_value",
                lambda value: first_candidate(value).update({"tempo_value": None}),
            ),
        ]
        for pointer, mutate in mutations:
            with self.subTest(pointer=pointer):
                self.assert_semantic_mutation_fails(mutate, pointer)

    def assert_rule_failure(
        self, failures: list[checker.Failure], rule: str, pointer: str
    ) -> None:
        self.assertTrue(
            any(
                path.startswith(pointer) and rule in message
                for path, message in failures
            ),
            msg=f"{rule} mutant did not fail {pointer}: {failures}",
        )

    def test_post_em5_rule_mutants_are_rejected(self) -> None:
        draw_root = chess.Board("7k/8/8/8/8/8/8/KB6 w - - 0 1")
        draw_consequence: checker.JsonObject = {
            "class": "positional",
            "plies": 1,
            "terminal_draw": {"kind": "insufficient_material", "ply": 1},
            "settled": True,
        }
        failures: list[checker.Failure] = []
        _ = checker.check_line_consequence(
            draw_root, ["Ba2"], draw_consequence, "/move", failures, 1
        )
        self.assertEqual(failures, [])
        draw_mutant = deepcopy(draw_consequence)
        draw_mutant["terminal_draw"] = None
        failures.clear()
        _ = checker.check_line_consequence(
            draw_root, ["Ba2"], draw_mutant, "/move", failures, 1
        )
        self.assert_rule_failure(failures, "P1", "/move/consequence/terminal_draw")

        stalemate_root = chess.Board("8/8/8/8/6q1/6R1/5k2/7K b - - 0 1")
        skipped_move: checker.JsonObject = {
            "san": "Qxg3",
            "verdict": "blunder",
            "refutation": None,
            "refutation_skipped": "game_over",
        }
        failures.clear()
        checker.check_refutation_skip(skipped_move, "/move", stalemate_root, failures)
        self.assertEqual(failures, [])
        skipped_mutant = deepcopy(skipped_move)
        skipped_mutant["refutation_skipped"] = None
        failures.clear()
        checker.check_refutation_skip(skipped_mutant, "/move", stalemate_root, failures)
        self.assert_rule_failure(failures, "P2", "/move/refutation_skipped")

        walk_root = chess.Board(START_FEN)
        extended_line: list[checker.JsonValue] = [
            "e4",
            "e5",
            "Nf3",
            "Nc6",
            "Bb5",
            "a6",
            "Bxc6",
        ]
        consequence: checker.JsonObject = {
            "class": "positional",
            "plies": 1,
            "terminal_draw": None,
            "settled": True,
        }
        failures.clear()
        _ = checker.check_line_consequence(
            walk_root, extended_line, consequence, "/move", failures, 7
        )
        self.assertEqual(failures, [])
        failures.clear()
        _ = checker.check_line_consequence(
            walk_root, extended_line, consequence, "/move", failures, 6
        )
        self.assert_rule_failure(failures, "P3", "/move/walked_plies")

        def check_tempo(value: checker.JsonObject) -> list[checker.Failure]:
            result: list[checker.Failure] = []
            checker.check_tempo_and_positional_semantics(
                value, chess.Board(START_FEN), result
            )
            return result

        def ledger_ply(value: checker.JsonObject, index: int) -> checker.JsonObject:
            ledger = object_field(first_candidate(value), "tempo_ledger")
            return object_value(array_field(ledger, "plies")[index])

        mutations: list[tuple[str, str, Mutation]] = [
            (
                "P4",
                "/candidates/0/tempo_ledger/plies/1/reply_gain/through_ply",
                lambda value: object_field(ledger_ply(value, 1), "reply_gain").update(
                    {"through_ply": 1}
                ),
            ),
            (
                "P5",
                "/candidates/0/tempo_ledger/plies/1/reply_gain/gains",
                lambda value: object_field(ledger_ply(value, 1), "reply_gain").update(
                    {"gains": True}
                ),
            ),
            (
                "P6",
                "/candidates/0/tempo_ledger/plies/1/forced_test/forced",
                lambda value: object_field(ledger_ply(value, 1), "forced_test").update(
                    {"forced": False}
                ),
            ),
            (
                "P7",
                "/candidates/0/tempo_ledger/plies/1/label",
                lambda value: ledger_ply(value, 1).update({"label": "gaining_reply"}),
            ),
            (
                "P8",
                "/candidates/0/tempo_ledger/runs/0/ending",
                lambda value: object_value(
                    array_field(
                        object_field(first_candidate(value), "tempo_ledger"),
                        "runs",
                    )[0]
                ).update({"ending": "answered"}),
            ),
            (
                "P9",
                "/candidates/0/tempo_ledger/runs/0/outcome",
                lambda value: object_value(
                    array_field(
                        object_field(first_candidate(value), "tempo_ledger"),
                        "runs",
                    )[0]
                ).update({"outcome": "evaporated"}),
            ),
        ]
        for rule, pointer, mutate in mutations:
            with self.subTest(rule=rule):
                mutant = deepcopy(tempo_semantic_report())
                mutate(mutant)
                self.assert_rule_failure(check_tempo(mutant), rule, pointer)

        short_report = deepcopy(tempo_semantic_report())
        short_ledger = object_field(first_candidate(short_report), "tempo_ledger")
        short_ply_one = object_value(array_field(short_ledger, "plies")[0])
        short_ply_two = object_value(array_field(short_ledger, "plies")[1])
        short_forced = object_field(short_ply_two, "forced_test")
        short_forced["lines"] = array_field(short_forced, "lines")[:4]
        short_forced["forced"] = None
        short_ply_one["tempo_gained"] = False
        short_ply_two["label"] = "unresolved_reply"
        object_field(short_ledger, "tempi")["white"] = 0
        short_ledger["runs"] = []
        self.assertEqual(check_tempo(short_report), [])
        short_mutant = deepcopy(short_report)
        object_field(ledger_ply(short_mutant, 1), "forced_test")["forced"] = True
        self.assert_rule_failure(
            check_tempo(short_mutant),
            "P6",
            "/candidates/0/tempo_ledger/plies/1/forced_test/forced",
        )

        capture_root = chess.Board("7k/8/8/4p3/3P4/8/8/K7 w - - 0 1")
        capture_line: list[checker.JsonValue] = ["Kb2", "Kg7", "Kc2", "exd4"]
        capture_walk = checker.replay_walk(
            capture_root, {"line": capture_line, "walked_plies": 4}
        )
        assert capture_walk is not None
        capture_boards, capture_moves = capture_walk
        weak_feature: checker.JsonObject = {
            "tag": "isolated_pawn",
            "side": "white",
            "pieces": [{"square": "d4", "piece": "P"}],
            "file": "d",
            "value": None,
        }
        captured_weakness: checker.JsonObject = {
            **weak_feature,
            "ply": 4,
            "change": "disappears",
            "net_change": True,
            "captured": True,
            "in_use": True,
        }
        self.assertIsNone(checker.favoured_side(captured_weakness))
        capture_report: checker.JsonObject = {
            "position": {"positional": [weak_feature]}
        }
        capture_assessment: checker.JsonObject = {
            "line": capture_line,
            "walked_plies": 4,
            "positional": [captured_weakness],
        }
        failures.clear()
        checker.check_positional_relations(
            capture_report,
            capture_assessment,
            "/move",
            capture_boards,
            capture_moves,
            False,
            failures,
        )
        self.assertEqual(failures, [])
        captured_mutant = deepcopy(capture_assessment)
        object_value(array_field(captured_mutant, "positional")[0])["captured"] = False
        failures.clear()
        checker.check_positional_relations(
            capture_report,
            captured_mutant,
            "/move",
            capture_boards,
            capture_moves,
            False,
            failures,
        )
        self.assert_rule_failure(failures, "P10", "/move/positional/0/captured")

        shelter_root = chess.Board("7k/8/8/8/4r3/8/4P3/1N2K1N1 w - - 0 1")
        shelter_line: list[checker.JsonValue] = [
            "Nf3",
            "Kh7",
            "Nc3",
            "Rxe2+",
        ]
        shelter_walk = checker.replay_walk(
            shelter_root, {"line": shelter_line, "walked_plies": 4}
        )
        assert shelter_walk is not None
        shelter_boards, shelter_moves = shelter_walk
        shelter_change: checker.JsonObject = {
            "tag": "open_file_near_king",
            "side": "white",
            "pieces": [],
            "file": "e",
            "value": None,
            "ply": 3,
            "change": "appears",
            "net_change": True,
            "captured": False,
            "in_use": True,
        }
        shelter_report: checker.JsonObject = {"position": {"positional": []}}
        shelter_assessment: checker.JsonObject = {
            "line": shelter_line,
            "walked_plies": 4,
            "positional": [shelter_change],
        }
        failures.clear()
        checker.check_positional_relations(
            shelter_report,
            shelter_assessment,
            "/move",
            shelter_boards,
            shelter_moves,
            False,
            failures,
        )
        self.assertEqual(failures, [])
        shelter_mutant = deepcopy(shelter_assessment)
        object_value(array_field(shelter_mutant, "positional")[0])["in_use"] = False
        failures.clear()
        checker.check_positional_relations(
            shelter_report,
            shelter_mutant,
            "/move",
            shelter_boards,
            shelter_moves,
            False,
            failures,
        )
        self.assert_rule_failure(failures, "P11", "/move/positional/0/in_use")

    def test_tempo_value_labels_follow_frozen_unit_boundaries(self) -> None:
        expected = {
            -1: "zugzwang",
            0: "non_tempo",
            99: "non_tempo",
            100: None,
            200: None,
            201: "tempo_dependent",
        }
        for units, label in expected.items():
            with self.subTest(units=units):
                passes = (1000 - units) / 2000
                value = units / 2000
                failures: list[checker.Failure] = []
                checker.check_tempo_value(
                    {
                        "side": "white",
                        "moves": 0.5,
                        "passes": passes,
                        "value": value,
                        "label": label,
                        "source": "engine",
                    },
                    "/tempo",
                    chess.WHITE,
                    1000,
                    False,
                    failures,
                )
                self.assertEqual(failures, [])
        failures = []
        checker.check_tempo_value(
            {"side": "white"}, "/tempo", chess.WHITE, None, True, failures
        )
        self.assertEqual([path for path, _ in failures], ["/tempo"])

    def test_net_change_and_use_fields_are_checked_independently(self) -> None:
        root = chess.Board(START_FEN)
        assessment: checker.JsonObject = {"line": ["e4"], "walked_plies": 1}
        walk = checker.replay_walk(root, assessment)
        assert walk is not None
        boards, moves = walk
        feature: checker.JsonObject = {
            "tag": "bishop_pair",
            "side": "white",
            "pieces": [],
            "file": None,
            "value": None,
        }
        change: checker.JsonObject = {
            **feature,
            "ply": 1,
            "change": "appears",
            "net_change": False,
            "captured": False,
            "in_use": None,
        }
        assessment["positional"] = [change]
        report: checker.JsonObject = {"position": {"positional": [feature]}}
        failures: list[checker.Failure] = []
        checker.check_positional_relations(
            report, assessment, "/candidate", boards, moves, False, failures
        )
        self.assertEqual(failures, [])

        change["net_change"] = True
        failures.clear()
        checker.check_positional_relations(
            report, assessment, "/candidate", boards, moves, False, failures
        )
        self.assertTrue(any(path.endswith("/net_change") for path, _ in failures))

        object_field(report, "position")["positional"] = []
        change["net_change"] = True
        change["in_use"] = True
        failures.clear()
        checker.check_positional_relations(
            report, assessment, "/candidate", boards, moves, False, failures
        )
        self.assertEqual(failures, [])
        change["in_use"] = False
        failures.clear()
        checker.check_positional_relations(
            report, assessment, "/candidate", boards, moves, False, failures
        )
        self.assertTrue(any(path.endswith("/in_use") for path, _ in failures))

    def test_initiative_conversion_uses_material_and_favoured_changes(self) -> None:
        report = tempo_semantic_report()
        candidate = first_candidate(report)
        initial_line: list[checker.JsonValue] = ["e4", "d5", "exd5"]
        candidate["line"] = initial_line
        candidate["walked_plies"] = 3
        ledger = object_field(candidate, "tempo_ledger")
        plies: list[checker.JsonValue] = [
            {
                "ply": 1,
                "san": "e4",
                "side": "white",
                "threatening": True,
                "by_check": False,
                "threat_rank": 2,
                "stops_threat": None,
                "label": "threatening",
                "tempo_gained": True,
                "reply_gain": None,
                "forced_test": None,
            },
            {
                "ply": 2,
                "san": "d5",
                "side": "black",
                "threatening": False,
                "by_check": False,
                "threat_rank": 0,
                "stops_threat": True,
                "label": "pure_reply",
                "tempo_gained": False,
                "reply_gain": {
                    "material": -1,
                    "mates": False,
                    "through_ply": 3,
                    "settled": True,
                    "gains": False,
                },
                "forced_test": forced_test_for_e5(),
            },
            {
                "ply": 3,
                "san": "exd5",
                "side": "white",
                "threatening": False,
                "by_check": False,
                "threat_rank": 0,
                "stops_threat": None,
                "label": "neither",
                "tempo_gained": False,
                "reply_gain": None,
                "forced_test": None,
            },
        ]
        ledger["plies"] = plies
        tempi: checker.JsonObject = {"white": 1, "black": 0}
        ledger["tempi"] = tempi
        run: checker.JsonObject = {
            "holder": "white",
            "start_ply": 1,
            "end_ply": 3,
            "tempi": 1,
            "ending": "spent",
            "outcome": "converted",
            "source": "rule",
        }
        runs: list[checker.JsonValue] = [run]
        ledger["runs"] = runs
        failures: list[checker.Failure] = []
        checker.check_tempo_and_positional_semantics(
            report, chess.Board(START_FEN), failures
        )
        self.assertEqual(failures, [])

        positional: list[checker.JsonValue] = [
            {
                "tag": "bishop_pair",
                "side": "white",
                "pieces": [],
                "ply": 1,
                "change": "appears",
                "net_change": True,
                "captured": False,
                "in_use": True,
            }
        ]
        candidate["positional"] = positional
        run["end_ply"] = 2
        run["ending"] = "unfinished"
        run["outcome"] = "unknown"
        line: list[checker.JsonValue] = ["e4", "e5"]
        candidate["line"] = line
        candidate["walked_plies"] = 2
        ledger["plies"] = array_field(
            object_field(first_candidate(tempo_semantic_report()), "tempo_ledger"),
            "plies",
        )
        ledger["tempi"] = tempi
        failures.clear()
        checker.check_tempo_and_positional_semantics(
            report, chess.Board(START_FEN), failures
        )
        self.assertEqual(failures, [])

    def test_en_passant_king_zone_attacker_uses_landing_square(self) -> None:
        root = chess.Board("8/8/3B4/4Pp2/6k1/8/8/K7 w - f6 0 1")
        assessment: checker.JsonObject = {"line": ["exf6"], "walked_plies": 1}
        walk = checker.replay_walk(root, assessment)
        assert walk is not None
        boards, moves = walk
        change: checker.JsonObject = {
            "tag": "king_zone_attacker",
            "side": "black",
            "change": "appears",
            "ply": 1,
            "pieces": [{"square": "d6", "piece": "B"}],
        }

        self.assertFalse(checker.use_test(change, boards, moves, False))

    def test_en_passant_file_shelter_keeps_removed_square_behavior(self) -> None:
        root = chess.Board("8/8/3B4/4Pp2/6k1/8/8/K7 w - f6 0 1")
        assessment: checker.JsonObject = {"line": ["exf6"], "walked_plies": 1}
        walk = checker.replay_walk(root, assessment)
        assert walk is not None
        boards, moves = walk

        self.assertTrue(checker.king_pressed(chess.BLACK, 1, boards, moves, "f"))

    def test_use_test_covers_tags_timing_pressure_and_superseding(self) -> None:
        def is_used(
            fen: str,
            line: list[str],
            tag: str,
            side: str,
            change: str,
            ply: int,
            squares: list[str] | None = None,
            tempo_dependent: bool = False,
            file_name: str | None = None,
        ) -> bool:
            root = chess.Board(fen)
            walk_line: list[checker.JsonValue] = [move for move in line]
            walk_assessment: checker.JsonObject = {
                "line": walk_line,
                "walked_plies": len(line),
            }
            walk = checker.replay_walk(root, walk_assessment)
            assert walk is not None
            boards, moves = walk
            pieces: list[checker.JsonValue] = [
                {"square": square, "piece": "P"} for square in squares or []
            ]
            feature: checker.JsonObject = {
                "tag": tag,
                "side": side,
                "change": change,
                "ply": ply,
                "pieces": pieces,
                "file": file_name,
            }
            return checker.use_test(feature, boards, moves, tempo_dependent)

        self.assertTrue(
            is_used(START_FEN, ["e4"], "bishop_pair", "white", "appears", 1)
        )
        self.assertFalse(
            is_used(START_FEN, ["e4"], "pawn_islands", "white", "appears", 1)
        )
        self.assertFalse(
            is_used(START_FEN, ["e4"], "mobility_lead", "white", "appears", 1)
        )
        self.assertTrue(
            is_used(
                START_FEN,
                ["e4"],
                "mobility_lead",
                "white",
                "appears",
                1,
                tempo_dependent=True,
            )
        )
        for tag in (
            "isolated_pawn",
            "passed_pawn",
            "castling_kingside",
            "rook_open_file",
        ):
            self.assertTrue(is_used(START_FEN, ["e4"], tag, "white", "appears", 1))

        weak_capture = "7k/8/8/4p3/3P4/8/8/K7 w - - 0 1"
        self.assertTrue(
            is_used(
                weak_capture,
                ["Kb2", "Kg7", "Kc2", "exd4"],
                "isolated_pawn",
                "white",
                "appears",
                3,
                ["d4"],
            )
        )
        weak_attack = "7k/8/4n3/8/3P4/8/8/K7 w - - 0 1"
        self.assertTrue(
            is_used(
                weak_attack,
                ["Kb1", "Kg7", "Kc2"],
                "isolated_pawn",
                "white",
                "appears",
                3,
                ["d4"],
            )
        )
        self.assertTrue(
            is_used(
                "7k/8/8/4p3/3P4/8/8/K7 b - - 0 1",
                ["Kg7", "Kb2", "exd4"],
                "isolated_pawn",
                "white",
                "disappears",
                3,
                ["d4"],
            )
        )
        self.assertTrue(
            is_used(
                "7k/8/8/8/3P4/8/8/K7 w - - 0 1",
                ["Kb1", "Kg7", "Kc2", "Kh6", "d5"],
                "passed_pawn",
                "white",
                "appears",
                3,
                ["d4"],
            )
        )
        self.assertTrue(
            is_used(
                "7k/8/8/8/3P4/8/8/K7 w - - 0 1",
                ["Kb1", "Kg7", "d5"],
                "passed_pawn",
                "white",
                "disappears",
                3,
                ["d4"],
            )
        )
        self.assertTrue(
            is_used(
                "7k/8/8/8/8/8/8/R1K5 w - - 0 1",
                ["Kb1", "Kg7", "Kc2", "Kh6", "Ra2"],
                "rook_open_file",
                "white",
                "appears",
                3,
                ["a1"],
            )
        )
        self.assertFalse(
            is_used(
                START_FEN,
                ["Nf3", "Nf6", "g3"],
                "open_file_near_king",
                "white",
                "appears",
                3,
            )
        )
        self.assertFalse(
            is_used(
                START_FEN,
                ["Nf3", "e6", "d4", "Bb4+"],
                "open_file_near_king",
                "white",
                "appears",
                3,
                file_name="e",
            )
        )
        self.assertTrue(
            is_used(
                "4r2k/8/8/8/8/8/r3P3/4K3 w - - 0 1",
                ["Kd1"],
                "king_zone_attacker",
                "white",
                "appears",
                1,
            )
        )
        self.assertFalse(
            is_used(
                START_FEN,
                ["e4"],
                "king_zone_attacker",
                "white",
                "disappears",
                1,
            )
        )
        self.assertFalse(
            is_used(
                "7k/8/5KQ1/8/8/8/8/8 w - - 0 1",
                ["Qg7#"],
                "outpost",
                "white",
                "appears",
                1,
            )
        )
        self.assertFalse(
            is_used(
                "7k/8/8/8/8/8/8/R1K5 w - - 0 1",
                ["Kb1", "Kg7", "Kc2"],
                "rook_open_file",
                "white",
                "disappears",
                3,
                ["a1"],
            )
        )
        f06_root = "8/pp2R2p/3BNkb1/3P1p2/7p/8/PP1rr2P/5R1K w - - 1 32"
        f06_line = ["Rxb7", "Rxe6", "dxe6", "Rxd6", "Re1", "Rd2"]
        self.assertFalse(
            is_used(
                f06_root,
                f06_line,
                "open_file_near_king",
                "black",
                "disappears",
                3,
                ["f6"],
            )
        )
        self.assertFalse(
            is_used(
                f06_root,
                f06_line,
                "open_file_near_king",
                "black",
                "appears",
                3,
                ["f6"],
                file_name="f",
            )
        )
        self.assertTrue(
            is_used(
                "7k/8/8/8/4r3/8/4P3/1N2K1N1 w - - 0 1",
                ["Nf3", "Kh7", "Nc3", "Rxe2+"],
                "open_file_near_king",
                "white",
                "appears",
                3,
                file_name="e",
            )
        )
        self.assertTrue(
            is_used(
                "7k/8/8/8/8/8/8/K6r w - - 0 1",
                ["Kb2", "Rh2"],
                "king_zone_attacker",
                "white",
                "appears",
                2,
                ["h2"],
            )
        )
        self.assertFalse(
            is_used(
                "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
                ["O-O"],
                "castling_kingside",
                "white",
                "disappears",
                1,
            )
        )
        self.assertFalse(
            is_used(
                START_FEN,
                ["e4", "d5", "exd5"],
                "material_ahead",
                "white",
                "disappears",
                3,
            )
        )
        self.assertFalse(
            is_used(
                START_FEN,
                ["e4", "Nf6", "Ke2"],
                "open_file_near_king",
                "white",
                "disappears",
                3,
            )
        )


if __name__ == "__main__":
    _ = unittest.main()
