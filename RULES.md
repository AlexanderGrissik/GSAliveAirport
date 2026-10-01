# Project Coding Rules

These rules apply to all source files in this project.

## 1. One Root Class Per File Pair

Each `.cpp`/`.h` pair must define exactly **one** root class.

## 2. No Free Globals

No global methods or global variables outside of a class. Everything must be a member of some class (Java-style encapsulation).

### 2.1 Exceptions

- `main()` — allowed as a free function.
- Any other exception **must** be explicitly approved by the user and documented in this section.

## 3. Header-Only Files

A `.h` file **may** exist without a corresponding `.cpp` file, provided the header defines a class (e.g. a template class, or a header-only utility class).

## 4. Non-Root Classes

Any class that is needed but is **not** the root class of its `.h` file must either:
- have its own dedicated `.h` (and optionally `.cpp`) file, or
- be declared as a **nested class** inside the root class.

## 5. Brace Placement

- **Class definition** opening brace → **next line**.
  ```cpp
  class Foo
  {
  };
  ```
- **Member function definition** in a `.cpp` file → **next line**.
  ```cpp
  void Foo::bar()
  {
      // ...
  }
  ```
- **All other** opening braces (`if`, `while`, `for`, `switch`, lambdas, etc.) → **same line**.
  ```cpp
  if (condition) {
      // ...
  }
  for (auto& x : items) {
      // ...
  }
  ```

## 6. Naming Conventions

- Static constants must begin with `s_` (e.g., `s_speedLimit`, `s_maxCount`).
- Non-static (local) constants must be ALL UPPER CASE (e.g., `MAX_RETRIES`, `TWO_PI`).
- Static member variables must begin with `s_` (e.g., `s_instance`).
- Non-static member variables must begin with `m_` (e.g., `m_speed`).
- Methods: PascalCase (e.g., `GetSpeed`).
- Class names must begin with `GS` (e.g., `GSAircraft`, `GSGeography`).
- Namespaces: `NS_` prefix (e.g., `NS_GSAliveAirport`).

## 7. One-Liner Methods

One-liner methods (single statement body) should be defined inline in the `.h` file, unless the resulting line would exceed 120 characters. In that case, place the definition in the `.cpp` file.

## 8. Line Length

Code lines should be kept on a single line unless they exceed 180 characters or the logic is too complex to reasonably express within 180 characters. Do not break lines unnecessarily.
