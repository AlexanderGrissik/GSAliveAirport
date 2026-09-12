# Project Rules for AI Assistant

## 1. Follow Project Coding Rules

Before writing, editing, or reviewing any code in this project, read and follow every rule defined in `RULES.md` at the project root.

If a proposed change would violate any rule, flag it to the user before proceeding.

## 2. Approval Before File Changes

Before making any code or file changes in this project:

1. Investigate the relevant code, files, configuration, and surrounding context first.
2. Explain to the user:
   - what you found;
   - the code or file changes you propose;
   - every file you expect to affect; and
   - any important consequences, risks, tradeoffs, or alternatives.
3. Wait for the user's explicit approval of the proposed changes.
4. Do not edit, create, delete, rename, move, or otherwise modify any file before that approval is given.
5. After approval, make only the changes the user approved.
6. If the implementation needs to differ materially from the approved proposal, stop, explain the revised plan and its effects, and ask for explicit approval again before continuing.

The user's request to investigate, diagnose, review, or explain something is not approval to modify files. Approval must clearly authorize the proposed file changes.

## 3. No Git Write Operations Without Explicit Request

Never execute any git write operation (commit, push, reset, rebase, amend, checkout, branch delete, tag, etc.) unless the user explicitly asks for that specific action in their last message. Read-only git commands (status, log, diff, show) are always allowed.

## 4. No Auto-Continue to Next Subtask

Never proceed to the next subtask, step, or any further action without the user's explicit approval. After completing an approved change, stop and wait for the user to direct the next step.
## 5. No Unrequested Fixes

Compiling, building, testing, or diagnosing code is not authorization to modify it. When a build fails, report the errors and stop. Do not edit any file to fix them unless the user explicitly asks for that specific fix in their message.

## 6. Compile/Build Output

If the user asks to compile or build and there are errors or warnings, report them and stop. Do not fix them. Wait for the user to say what to do next.

## 7. Execute Exactly What Is Requested

Execute exactly what the user specifies, nothing more, nothing less. No assumed next steps, no "while I'm here" extras, no unrequested fixes. Each action only when you explicitly request it.
