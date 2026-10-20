import pathlib
import tempfile
import unittest

from tools.tuning import artifacts


class ArtifactTest(unittest.TestCase):
    def test_provenance_covers_all_production_tuning_modules(self):
        tools = pathlib.Path(artifacts.__file__).resolve().parents[1]
        sources = {"__init__.py"} | {
            path.relative_to(tools).as_posix()
            for path in (tools / "tuning").glob("*.py")
        }
        self.assertEqual(set(artifacts.tool_record()), sources)

    def test_corrupt_and_legacy_artifacts_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "fit.json"
            artifact = artifacts.write_artifact(path, {"kind": "fit", "value": 1})
            artifact["value"] = 2
            artifacts.atomic_write_json(path, artifact)
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                artifacts.read_artifact(path)
            artifact["format_version"] = 4
            artifact["artifact_id"] = artifacts.artifact_id(artifact)
            artifacts.atomic_write_json(path, artifact)
            with self.assertRaisesRegex(ValueError, "prepare and fit into new output directories"):
                artifacts.read_artifact(path)


if __name__ == "__main__":
    unittest.main()
