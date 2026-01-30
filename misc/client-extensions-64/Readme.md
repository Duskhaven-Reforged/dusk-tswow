# Discord Social SDK IPC Bridge (Test Implementation)

This project is a **test implementation of an external helper program** that communicates with a **32-bit DLL injected into World of Warcraft**.

Its purpose is to bridge WoW with the **Discord Social SDK**, enabling:

- 🎙 **Voice Calls**
- 🎮 **Rich Presence**
- 🚧 *(Planned)* Messaging & Friends integration

The external process handles Discord IPC and forwards requests from the injected DLL, keeping game-side code lightweight and isolated from Discord SDK complexity.

---

## 🧩 Architecture Overview

```
World of Warcraft (32-bit)
        │
        │  IPC (Named Pipe)
        ▼
External Helper Process (this program)
        │
        │  Discord IPC
        ▼
Discord Social SDK
```

---

## 📁 Project Structure

### ✏️ Editable / Configurable Files

These files are intended to be modified for setup or extension:

- **main.cpp**
  - Update:
    - `APPLICATION_ID`
    - `PIPE_NAME`

- **DiscordManager/**
  - Core Discord IPC and SDK handling logic, extend class for new SDK functions

- **handlers/Opcodes.h**
  - Defines IPC opcodes used for communication between the DLL and the external process, prefixed with CMSG/SMSG for Client/Server Message
  

---

### ➕ Adding New Handlers

To implement new IPC handlers or features:

1. Create new handler files under:
   ```
   handlers/<category>/<handler>.*
   ```

2. Register the new opcode in:
   - `handlers/Opcodes.h`
   - All new categories should increment the 100s value to get to a new set of opcodes

This modular structure allows clean expansion for future features.

---

### ⚠️ Do Not Edit (Unless You Really Know What You’re Doing)

The following files are **low-level and protocol-critical**.  
Modifying them without deep understanding may break IPC or Discord integration:

- `IPC/*`
- `cdiscord.h`
- `discordpp.h`
- `handlers/HandlerRegistry.*`
- `handlers/VirtualHandler.h`

---

## 🚀 Future Ideas

- 💬 Discord messaging
- 👥 Friends & presence syncing
- 📊 Extended Rich Presence support

---

## 📜 Disclaimer

This project is provided as-is for experimentation and educational purposes.  
Use at your own risk.


## Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
```