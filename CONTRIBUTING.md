# Contributing

Use the naming and selector contract in [examples](docs/examples.md). Preserve source-specific licenses and do not commit build output, credentials or resolved dependencies. Run `python3 -m unittest discover -s tests -v`, `python3 scripts/validate_repo.py` and `git diff --check` in Ubuntu Bash. For Windows PowerShell use `py -3` instead of `python3`.

Report static, configuration, compilation and board results separately. Framework changes need official target-support evidence and an exact commit pin; preview support must be explicitly approved. Public release tags require complete builds and archive validation as described in [CI](docs/ci.md).
