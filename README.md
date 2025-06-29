# Word Guess Game in Win32

This repository contains an academic project developed as part of a practical assignment for the **Operating Systems 2** course. The project implements a multiplayer word-guessing game using **C and the Win32 API**, applying key operating systems concepts such as inter-process communication, shared memory, and multithreading.

Players (human or bot) connect to an referee, who coordinates the game. Letters are sent to the players, and they try to guess valid words. A separate panel program can be launched to visualize the game state in real time.

> ⚠️ The project is intended for **Windows only** due to its use of Win32 libraries and Windows-specific system calls.

## System Components

The system consists of four main programs:

1. **Arbitro (Manager/referee)**
   - Accepts player connections.
   - Starts the game when enough players have joined.
   - Sends and validates messages.
   - Updates shared memory for the panel.
   - Processes administrator commands.

2. **JogoUI (Player)**
   - Command-line interface for human players.
   - Receives game state and sends letter guesses.
   - Handles messages from the referee.

3. **Bot**
   - Simulates a player with random or semi-intelligent guesses.
   - Difficulty level affects guess quality.

4. **Painel (Dashboard)**
   - Displays the game state in real time using a Win32 GUI.
   - Pulls data from shared memory.

Each component is compiled and executed separately, communicating via **named pipes** and shared memory segments.

---

## Overview

The project was developed in **C** and addresses the following main topics:

1. **IPC Mechanisms:**
   - Communication using **named pipes**.
   - Shared data through **shared memory**.
   - **Events** for state coordination.

2. **Multithreading and Synchronization:**
   - Use of **threads** to handle concurrent players.
   - **Mutexes** to protect shared resources.

3. **Structured Message System:**
   - Messages follow a defined type structure (e.g., `JOIN_MSG`, `ERROR_MSG`, etc.).
   - A common header file (`data.h`) defines all data structures and message types.

4. **Modular Architecture:**
   - Clear separation between logic, communication, UI, and game state.
   - Code structured across multiple files for scalability and maintainability.

---

### Repository Structure

```
arbitro/                 -> Main game coordinator (referee)
|-- main.c               -> Entry point, initializes the referee
|-- game.c               -> Core game logic and registry management
|-- players.c            -> Player connection and management
|-- threads.c            -> Thread creation and communication handling
|-- utils.c              -> Utility functions (commands, messaging, setup)

jogoUI/                  -> Human player interface
|-- main.c
|-- utils.c

bot/                     -> Automated bot player
|-- bot.c                -> Bot logic (random/dictionary-based guesses)

painel/                  -> Game status visualizer
|-- panel.c              -> Win32 GUI that reads from shared memory

common/                  -> Shared definitions
|-- data.h               -> Structs, constants, and message types used across programs
```

---

## Authors

- [Rafael Pereira](https://github.com/rafaelp3re1ra)
- [Sebastian Vigas](https://github.com/sebie12)
