# Contributing Guide

Thanks for contributing to Air Dribble Rating Bot.

## 🧱 Project rules

- Use C++23
- Follow existing naming conventions
- Prefer modular design
- Keep handlers and commands separated

## Workflow

Before starting work on a feature or command:

1. Check existing GitHub Issues and Projects.
2. Open a new issue if the task does not exist yet.
3. Assign yourself to the issue.
4. Create a feature branch:
   ```bash
   git checkout -b feature/my-feature
   ```
5. Implement your changes.
6. Build and test the project.
7. Open a Pull Request linked to the issue.

## ➕ Adding a command

1. Create a new class inheriting `ICommand`
2. Implement required methods:

```cpp
std::string get_name() const override;
dpp::slashcommand definition(dpp::snowflake bot_id) const override;
std::expected<std::string, std::string> execute(const dpp::slashcommand_t& event) const override;
```

3. Register in CommandHandler

## 🧪 Testing

Build and run before pushing:

```bash
# Linux
make
make -j$(nproc) release
```

Ensure no warnings.

## 📦 Pull Requests

- Describe changes clearly
- Keep commits focused and atomic
- Avoid unrelated changes in the same PR
- Do not commit config.json
- Reference the related issue when possible
