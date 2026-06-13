# Architecture

## Overview

The bot is structured in 5 layers:

### 1. Core (main.cpp)

- Loads config
- Initializes DPP client

### 2. Handlers

- CommandHandler → manages slash commands
- EventHandler → handles Discord events

### 3. Commands

Each command is an independent class implementing ICommand.

### 4. Utils

Utils functions for commands.

### 5. Saved data (data/*.json)

Here is where all the data is saved.

## Design philosophy

- No global state (except bot instance)
- Commands are self-contained
- Strong separation of logic

## Data flow

User → Discord → EventHandler → CommandHandler → Command → Response
