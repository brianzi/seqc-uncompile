#!/usr/bin/env python3
"""
Manifest loader for SeqC differential testing framework.

Supports two manifest formats:
- v1.0: Flat array of test cases (backward compatible)
- v2.0: Hierarchical groups with imports, definitions, and metadata

Key features:
- @reference syntax for reusing definitions
- Multi-device expansion (one test → multiple device variants)
- Import/merge from multiple manifest files
- Auto-tagging by device type
- Flattening to canonical test list with hierarchical naming
"""

import json
import os
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple
from dataclasses import dataclass, field


@dataclass
class TestCase:
    """Canonical test case after all expansion and flattening."""
    name: str
    devtype: str
    code: Optional[str] = None
    file: Optional[str] = None
    index: int = 0
    options: str = ""
    samplerate: Optional[float] = None
    sequencer: Optional[str] = None
    comment: Optional[str] = None
    tags: List[str] = field(default_factory=list)
    groups: List[str] = field(default_factory=list)  # Hierarchical path

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for test runner compatibility."""
        d = {
            "name": self.name,
            "devtype": self.devtype,
            "index": self.index,
            "options": self.options,
        }
        if self.code is not None:
            d["code"] = self.code
        if self.file is not None:
            d["file"] = self.file
        if self.samplerate is not None:
            d["samplerate"] = self.samplerate
        if self.sequencer is not None:
            d["sequencer"] = self.sequencer
        return d


class ManifestLoader:
    """Load and process test manifests in v1.0 or v2.0 format."""

    def __init__(self, manifest_path: Path):
        self.manifest_path = Path(manifest_path)
        self.base_dir = self.manifest_path.parent
        self.definitions: Dict[str, Dict[str, Any]] = {}
        self.loaded_files: Set[Path] = set()  # Circular import detection

    def load(self) -> List[TestCase]:
        """Load manifest and return flattened list of test cases."""
        with open(self.manifest_path) as f:
            data = json.load(f)

        self.loaded_files.add(self.manifest_path.resolve())

        # Detect format version
        if isinstance(data, list):
            # v1.0: flat array
            return self._load_v1(data)
        elif isinstance(data, dict):
            version = data.get("version", "1.0")
            if version == "2.0":
                return self._load_v2(data)
            else:
                # v1.0 dict format (single group)
                return self._load_v1([data])
        else:
            raise ValueError(f"Invalid manifest format: {type(data)}")

    def _load_v1(self, test_list: List[Dict[str, Any]]) -> List[TestCase]:
        """Load v1.0 format (flat array of test cases)."""
        tests = []
        for i, test_dict in enumerate(test_list):
            # Auto-tag with device type
            devtype = test_dict.get("devtype", "")
            tags = self._auto_tags(devtype)
            
            test = TestCase(
                name=test_dict.get("name", f"test_{i}"),
                devtype=devtype,
                code=test_dict.get("code"),
                file=test_dict.get("file"),
                index=test_dict.get("index", 0),
                options=test_dict.get("options", ""),
                samplerate=test_dict.get("samplerate"),
                sequencer=test_dict.get("sequencer"),
                tags=tags,
                groups=["v1"],
            )
            tests.append(test)
        return tests

    def _load_v2(self, data: Dict[str, Any]) -> List[TestCase]:
        """Load v2.0 format (hierarchical groups with imports)."""
        # Load definitions
        self.definitions = data.get("definitions", {})

        # Process imports first
        tests = []
        for import_path in data.get("imports", []):
            tests.extend(self._load_import(import_path))

        # Process local groups
        for group in data.get("groups", []):
            tests.extend(self._load_group(group, parent_groups=[]))

        return tests

    def _load_import(self, import_path: str) -> List[TestCase]:
        """Load and merge an imported manifest file."""
        # Resolve path relative to current manifest's directory
        full_path = (self.base_dir / import_path).resolve()

        # Circular import check
        if full_path in self.loaded_files:
            raise ValueError(f"Circular import detected: {full_path}")

        # Load the imported manifest
        loader = ManifestLoader(full_path)
        return loader.load()

    def _load_group(
        self, group: Dict[str, Any], parent_groups: List[str]
    ) -> List[TestCase]:
        """Load a test group (recursive for nested groups)."""
        group_name = group.get("name", "unnamed")
        group_comment = group.get("comment")
        group_tags = group.get("tags", [])
        defaults = group.get("defaults")

        # Resolve defaults if it's a reference
        if isinstance(defaults, str) and defaults.startswith("@"):
            defaults = self._resolve_reference(defaults)

        # Current group path
        current_groups = parent_groups + [group_name]

        tests = []

        # Process nested groups first
        for nested_group in group.get("groups", []):
            tests.extend(self._load_group(nested_group, current_groups))

        # Process test cases
        for case in group.get("cases", []):
            # Check if this is a multi-device test
            if "devices" in case:
                tests.extend(
                    self._expand_multi_device(case, defaults, group_tags, current_groups)
                )
            else:
                tests.append(
                    self._load_test_case(case, defaults, group_tags, current_groups)
                )

        return tests

    def _expand_multi_device(
        self,
        case: Dict[str, Any],
        defaults: Optional[Dict[str, Any]],
        group_tags: List[str],
        current_groups: List[str],
    ) -> List[TestCase]:
        """Expand a test case with 'devices' into multiple tests."""
        devices = case["devices"]
        base_name = case.get("name", "unnamed")
        
        tests = []
        for device_spec in devices:
            # Resolve device spec if it's a reference
            if isinstance(device_spec, str) and device_spec.startswith("@"):
                device_config = self._resolve_reference(device_spec)
                device_suffix = device_spec[1:]  # Remove @ prefix
            else:
                device_config = device_spec
                device_suffix = device_config.get("devtype", "unknown")

            # Create test-specific config by merging
            test_config = case.copy()
            del test_config["devices"]
            
            # Merge: defaults < device_config < test_config
            merged = {}
            if defaults:
                merged.update(defaults)
            merged.update(device_config)
            merged.update(test_config)
            
            # Generate expanded name
            merged["name"] = f"{base_name}_{device_suffix.lower()}"
            
            tests.append(
                self._load_test_case(merged, None, group_tags, current_groups)
            )

        return tests

    def _load_test_case(
        self,
        case: Dict[str, Any],
        defaults: Optional[Dict[str, Any]],
        group_tags: List[str],
        current_groups: List[str],
    ) -> TestCase:
        """Load a single test case with defaults applied."""
        # Handle @base reference for inline override
        if "@base" in case:
            base_ref = case["@base"]
            base_config = self._resolve_reference(base_ref)
            merged = base_config.copy()
            merged.update(case)
            del merged["@base"]
            case = merged

        # Apply defaults
        if defaults:
            merged = defaults.copy()
            merged.update(case)
            case = merged

        # Collect tags: group tags + case tags + auto-tags
        devtype = case.get("devtype", "")
        tags = list(group_tags) + case.get("tags", []) + self._auto_tags(devtype)
        tags = list(dict.fromkeys(tags))  # Remove duplicates, preserve order

        # Generate hierarchical name
        groups_str = ":".join(current_groups)
        case_name = case.get("name", "unnamed")
        
        # Format device args for name suffix
        dev_args = [devtype]
        if case.get("sequencer"):
            dev_args.append(f"seq={case['sequencer']}")
        if case.get("samplerate"):
            # Format without the '+' sign: 2.4e9 instead of 2.4e+09
            sr_str = f"{case['samplerate']:.1e}".replace('e+0', 'e').replace('e-0', 'e-')
            dev_args.append(f"sr={sr_str}")
        if case.get("index", 0) != 0:
            dev_args.append(f"idx={case['index']}")
        dev_args_str = ", ".join(dev_args)
        
        full_name = f"{groups_str}:{case_name}[{dev_args_str}]"

        return TestCase(
            name=full_name,
            devtype=devtype,
            code=case.get("code"),
            file=case.get("file"),
            index=case.get("index", 0),
            options=case.get("options", ""),
            samplerate=case.get("samplerate"),
            sequencer=case.get("sequencer"),
            comment=case.get("comment"),
            tags=tags,
            groups=current_groups,
        )

    def _resolve_reference(self, ref: str) -> Dict[str, Any]:
        """Resolve a @reference to a definition."""
        if not ref.startswith("@"):
            raise ValueError(f"Invalid reference syntax: {ref}")

        key = ref[1:]  # Remove @ prefix
        if key not in self.definitions:
            raise ValueError(f"Undefined reference: {ref}")

        return self.definitions[key].copy()

    def _auto_tags(self, devtype: str) -> List[str]:
        """Generate automatic tags based on device type."""
        tags = []
        
        if not devtype:
            return tags

        # Device family tag
        if devtype.startswith("HDAWG"):
            tags.append("hdawg")
        elif devtype.startswith("SHFQA"):
            tags.append("shfqa")
        elif devtype.startswith("SHFSG"):
            tags.append("shfsg")
        elif devtype.startswith("SHFQC"):
            tags.append("shfqc")
        elif devtype.startswith("UHF"):
            tags.append("uhf")
        elif devtype.endswith("LI"):
            tags.append("lockin")

        # Add full devtype as tag
        tags.append(devtype.lower())

        return tags


def load_manifest(manifest_path: Path) -> List[TestCase]:
    """
    Load a test manifest and return flattened list of test cases.
    
    Supports both v1.0 (flat array) and v2.0 (hierarchical) formats.
    """
    loader = ManifestLoader(manifest_path)
    return loader.load()


def load_manifest_as_dicts(manifest_path: Path) -> List[Dict[str, Any]]:
    """
    Load manifest and return as list of dictionaries for backward compatibility.
    
    This is a convenience wrapper for existing test runners that expect
    the v1.0 dictionary format.
    """
    tests = load_manifest(manifest_path)
    return [test.to_dict() for test in tests]
