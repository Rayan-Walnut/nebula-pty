# Nebula PTY

A fast and lightweight PTY implementation for Electron applications.

## Features
- Native PTY implementation
- Fast and responsive terminal emulation
- Cross-platform support (Windows)
- Easy integration with Electron apps

## Prerequisites
- Node.js (version 14 or higher)
- Python (for node-gyp)
- CMake
- MinGW (on Windows)

## Installation
```bash
npm install nebula-pty
```

## Quick Start
```javascript
const { Terminal } = require('xterm');
const { WebTerminal } = require('nebula-pty');

// Create terminal
const term = new Terminal();
const ptyProcess = new WebTerminal();

// Start process
ptyProcess.startProcess({
    cols: term.cols,
    rows: term.rows
});

// Handle data
term.onData(data => ptyProcess.write(data));
ptyProcess.onData(data => term.write(data));
```

## Building from Source
1. Clone the repository
```bash
git clone https://github.com/yourusername/nebula-pty.git
cd nebula-pty
```

2. Check dependencies
```bash
./scripts/check.bat
```

3. Install dependencies and build
```bash
npm install
npm run build
```

## Development
### Setting up the development environment
```bash
npm install
```

### Running tests
```bash
npm test
```

## Contributing
Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of conduct, and the process for submitting pull requests.

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments
- [node-pty](https://github.com/microsoft/node-pty) - For inspiration
- [xterm.js](https://xtermjs.org/) - Terminal frontend