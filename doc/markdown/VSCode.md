# VSCode setup

Just basic notes to quickly get a configuration up and running.

## Extensions

### Required
- C/C++ Extension Pack (ms-vscode.cpptools-extension-pack)
- EditorConfig for VS Code (EditorConfig.EditorConfig)

### Recommended
- CMake Language Support (josetr.cmake-language-support-vscode)
- Comment Divder (stackbreak.comment-divider)
- Even Better TOML (tamasfe.even-better-toml)
- Better Comments (aaron-bond.better-comments)
- Doxygen Documentation Generator (cschlosser.doxdocgen)
  - **NOTE:** Has shown issues when rearranging lines. Need to investigate.
- Doxygen Runner (betwo.vscode-doxygen-runner)

### Required for Python
- Python (ms-python.python)
- Ruff (charliermarsh.ruff)
- Mypy Type Checker (ms-python.mypy-type-checker)

### Recommended for Python
- Better Jinja (samuelcolvin.jinjahtml)

## Settings

Example .vscode/settings.json
Some settings may make more sense in user settings.json

```json
{
    // Substitute {author} with git config --get user.name.
    "doxdocgen.generic.useGitUserName": false,
    // Substitute {email} with git config --get user.email.
    "doxdocgen.generic.useGitUserEmail": false,
    "doxdocgen.generic.authorEmail": "your@email.com",
    "doxdocgen.generic.authorName": "Your Name",
    "doxdocgen.file.fileTemplate": "@file {name}",
    "doxdocgen.file.copyrightTag": [
        "@copyright Copyright (c) {year}"
    ],
    // Additional file documentation. One tag per line will be added. Can template `{year}`, `{date}`, `{author}`, `{email}` and `{file}`. You have to specify the prefix.
    "doxdocgen.file.customTag": [
        "This file is part of the UCB project",
        "- SPDX-FileCopyrightText: © {year} {author} <{email}>",
        "- SPDX-License-Identifier: MIT",
    ],
    // The order to use for the file comment. Values can be used multiple times. Valid values are shown in default setting.
    "doxdocgen.file.fileOrder": [
        "file",
        "empty",
        "custom",
        "empty",
        "brief",
    ],
    "C_Cpp.default.cStandard": "c17",
    "C_Cpp.default.cppStandard": "c++17",
    // Stop polluting files.associations with junk
    "C_Cpp.autoAddFileAssociations": false,
    "comment-divider.languagesMap": {
        "cmake": [
            "#",
            "#"
        ]
    }
}
```

## Debugging

To debug a specific doctest example, add for instance this into launch.json

Example .vscode/launch.json

```json
{
    // Use IntelliSense to learn about possible attributes.
    // Hover to view descriptions of existing attributes.
    // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
    "version": "0.2.0",
    "configurations": [
    {
        "name": "Test",
        "type": "cppvsdbg",
        "request": "launch",
        "program": "${command:cmake.launchTargetPath}",
        "args": [ "-tc=unicode official normalization test" ],
        "stopAtEntry": false,
        "cwd": "${workspaceFolder}",
        "environment": [],
        "console": "internalConsole"
    }
    ]
}
```
