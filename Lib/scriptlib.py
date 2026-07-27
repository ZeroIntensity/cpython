import re
import tomllib
import argparse
from pathlib import Path
import venv
import tempfile
import sys
import subprocess

_SCRIPT_REGEX = re.compile(
    r"(?m)^# /// (?P<type>[a-zA-Z0-9-]+)$\s(?P<content>(^#(| .*)$\s)+)^# ///$"
)

def load_script_metadata(script: str) -> dict | None:
    matches = [
        match
        for match in _SCRIPT_REGEX.finditer(script)
        if match.group("type") == "script"
    ]
    if len(matches) > 1:
        raise ValueError(f"Multiple script blocks found")
    elif len(matches) == 1:
        content = "".join(
            line[2:] if line.startswith("# ") else line[1:]
            for line in matches[0].group("content").splitlines(keepends=True)
        )
        return tomllib.loads(content)
    else:
        return None


_VERSION_PATTERN = r"""
    v?
    (?:
        (?:(?P<epoch>[0-9]+)!)?                           # epoch
        (?P<release>[0-9]+(?:\.[0-9]+)*)                  # release segment
        (?P<pre>                                          # pre-release
            [-_\.]?
            (?P<pre_l>(a|b|c|rc|alpha|beta|pre|preview))
            [-_\.]?
            (?P<pre_n>[0-9]+)?
        )?
        (?P<post>                                         # post release
            (?:-(?P<post_n1>[0-9]+))
            |
            (?:
                [-_\.]?
                (?P<post_l>post|rev|r)
                [-_\.]?
                (?P<post_n2>[0-9]+)?
            )
        )?
        (?P<dev>                                          # dev release
            [-_\.]?
            (?P<dev_l>dev)
            [-_\.]?
            (?P<dev_n>[0-9]+)?
        )?
    )
    (?:\+(?P<local>[a-z0-9]+(?:[-_\.][a-z0-9]+)*))?       # local version
"""

_VERSION_REGEX = re.compile(
    r"^\s*" + _VERSION_PATTERN + r"\s*$",
    re.VERBOSE | re.IGNORECASE,
)

_PRE_ALIASES = {"alpha": "a", "beta": "b", "c": "rc", "pre": "rc", "preview": "rc"}
_POST_ALIASES = {"rev": "post", "r": "post"}


def _segment(letter: str | None, number: str, aliases: dict[str, str]):
    if letter is None:
        if number is None:
            return None
        letter = "post"  # implicit post release, e.g. "1.0-1"
    letter = letter.lower()
    return aliases.get(letter, letter), int(number or 0)


_PY_RELEASE_LEVELS = {"alpha": "a", "beta": "b", "candidate": "rc", "final": ""}


class Version:
    def __init__(self, version: str) -> None:
        self.raw = version
        match = _VERSION_REGEX.match(version)
        if match is None:
            raise ValueError(f"Invalid version: {version!r}")
        group = match.group

        epoch = int(group("epoch") or 0)
        release = tuple(int(part) for part in group("release").split("."))
        pre = _segment(group("pre_l"), group("pre_n"), _PRE_ALIASES)
        post = _segment(
            group("post_l"), group("post_n1") or group("post_n2"), _POST_ALIASES
        )
        dev = _segment(group("dev_l"), group("dev_n"), {})
        local = (
            tuple(
                int(part) if part.isdigit() else part
                for part in re.split(r"[-_.]", group("local").lower())
            )
            if group("local")
            else ()
        )

        self.epoch = epoch
        self.release = release
        self.pre = pre
        self.post = post
        self.dev = dev
        self.local = local

        release_key = release
        while release_key and release_key[-1] == 0:
            release_key = release_key[:-1]

        if pre is not None:
            pre_key = (0, *pre)
        elif post is None and dev is not None:
            pre_key = (-1, "", 0)
        else:
            pre_key = (1, "", 0)

        post_key = -1 if post is None else post[1]

        dev_key = (1, 0) if dev is None else (0, dev[1])

        local_key = tuple(
            (1, part, "") if isinstance(part, int) else (0, 0, part) for part in local
        )

        self._key = (epoch, release_key, pre_key, post_key, dev_key, local_key)

    def __str__(self) -> str:
        parts = []
        if self.epoch:
            parts.append(f"{self.epoch}!")
        parts.append(".".join(map(str, self.release)))
        if self.pre is not None:
            parts.append("%s%d" % self.pre)
        if self.post is not None:
            parts.append(f".post{self.post[1]}")
        if self.dev is not None:
            parts.append(f".dev{self.dev[1]}")
        if self.local:
            parts.append("+" + ".".join(map(str, self.local)))
        return "".join(parts)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, Version):
            return NotImplemented
        return self._key == other._key

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, Version):
            return NotImplemented
        return self._key < other._key

    def __le__(self, other: object) -> bool:
        if not isinstance(other, Version):
            return NotImplemented
        return self._key <= other._key

    def __gt__(self, other: object) -> bool:
        if not isinstance(other, Version):
            return NotImplemented
        return self._key > other._key

    def __ge__(self, other: object) -> bool:
        if not isinstance(other, Version):
            return NotImplemented
        return self._key >= other._key

    def is_compatible_with(self, other: Version) -> bool:
        if len(other.release) < 2:
            raise ValueError(f"~= requires at least two release segments: {other}")
        if other.local:
            raise ValueError(f"~= cannot be used with a local version: {other}")
        # Drop the last release component of *other* to form the == X.* prefix,
        # zero-padding our own release so that 1 matches a 1.0.0 prefix.
        stop = len(other.release) - 1
        release = self.release + (0,) * (stop - len(self.release))
        return (
            self.epoch == other.epoch
            and release[:stop] == other.release[:stop]
            and self >= other
        )

    def arbitrary_equal(self, other: Version):
        return self.raw == other.raw

    @classmethod
    def from_py_version(cls, info: tuple[int, int, int, str, int]) -> Version:
        major, minor, micro, releaselevel, serial = info
        try:
            letter = _PY_RELEASE_LEVELS[releaselevel]
        except KeyError:
            raise ValueError(f"unknown release level: {releaselevel!r}") from None
        pre = f"{letter}{serial}" if letter else ""
        return cls(f"{major}.{minor}.{micro}{pre}")


def parse_version_spec(version: str, /) -> tuple[str, Version]:
    if len(version) < 3:
        raise ValueError(f"{version!r} is too short for a version specifier")

    if version[0:2] in {"~=", "<=", ">=", "==", "!="}:
        return version[0:2], Version(version[2:])

    if version[0] in {"<", ">"}:
        return version[0], Version(version[1:])

    if version[0:3] == "===":
        return version[0:3], Version(version[3:])

    raise ValueError(f"{version!r} is not a valid version specifier")


def compare_python_version(
    version_spec: str, py_version_tuple: tuple[int, int, int, str, int], /
) -> bool:
    compare, version = parse_version_spec(version_spec)
    python_version = Version.from_py_version(py_version_tuple)

    compare_map = {
        "==": python_version.__eq__,
        "!=": python_version.__ne__,
        "<": python_version.__lt__,
        "<=": python_version.__le__,
        ">": python_version.__gt__,
        ">=": python_version.__ge__,
        "~=": python_version.is_compatible_with,
        "===": python_version.arbitrary_equal,
    }

    return compare_map[compare](version)


def run_script(path: str | Path, *, verbose: bool = False) -> None:
    path = Path(path)
    content = path.read_text()
    metadata = load_script_metadata(content) or {}

    if version := metadata.get("requires-python"):
        if not compare_python_version(version, sys.version_info[:5]):
            raise RuntimeError(
                f"This Python version is not compatible with {version!r}"
            )

    with tempfile.TemporaryDirectory() as temp_dir:
        venv_dir = Path(temp_dir) / "venv"
        venv.create(venv_dir, with_pip=True)
        script_file = path.copy_into(temp_dir)
        python = venv_dir / "bin" / "python"
        for dependency in metadata.get("dependencies") or []:
            subprocess.run(
                [python, "-m", "pip", "install", dependency],
                check=True,
                capture_output=not verbose,
            )

        subprocess.run([python, str(script_file.absolute())])


def _file_path(path_str: str) -> Path:
    path = Path(path_str)
    if not path.exists():
        raise argparse.ArgumentTypeError(f"The path '{path_str}' does not exist.")
    if not path.is_file():
        raise argparse.ArgumentTypeError(f"The path '{path_str}' is not a valid file.")
    return path


def main():
    parser = argparse.ArgumentParser(description='Run a script in an isolated environment.', suggest_on_error=True)
    parser.add_argument(
        "path",
        type=_file_path,
        help="Path to the script.",
    )
    parser.add_argument('-v', '--verbose', action='store_true', help="Enable verbosity.")
    args = parser.parse_args()
    run_script(args.path)


if __name__ == "__main__":
    main()
