# Nebula PTY

Nebula PTY is a modern and efficient pseudo-terminal library for Windows. This project requires `mingw` and `cmake` to build and run. Additionally, you need to clone the `winpty` dependency into the `deps` directory.

## Requirements

- `mingw`
- `nodejs`
- `python`

## Installation

1. Clone the repository:
    ```sh
    git clone https://github.com/yourusername/nebula-pty.git
    cd nebula-pty
    ```

2. Check dependencies:
    ```sh
    script/check.bat
    ```
    Install all dependencies.

3. Build the project:
    ```sh
    npm run build
    ```

4. Test with Electron:
    ```sh
    cd test/electron
    npm run start
    ```

## Usage

Once the library is built, you can use it to create and manage pseudo-terminals in your applications. Here is a basic example of how to use Nebula PTY with Node.js:

```javascript
const os = require('os');
const { Terminal } = require('xterm');
const { FitAddon } = require('xterm-addon-fit');
const { WebLinksAddon } = require('xterm-addon-web-links');
const { WebTerminal } = require('../../build/Release/terminal.node');

const term = new Terminal({
    cursorBlink: true,
    cursorStyle: 'block',
    fontFamily: 'Consolas, monospace',
    theme: {
        background: '#1e1e1e',
        foreground: '#d4d4d4',
        cursor: '#ffffff'
    }
});

const fitAddon = new FitAddon();
term.loadAddon(fitAddon);
term.loadAddon(new WebLinksAddon());

term.open(document.getElementById('terminal-container'));
fitAddon.fit();

const ptyProcess = new WebTerminal();
ptyProcess.startProcess({
    cols: term.cols,
    rows: term.rows,
    env: process.env
});

term.onData(data => ptyProcess.write(data));
ptyProcess.onData(data => term.write(data));

window.addEventListener('resize', () => {
    fitAddon.fit();
    ptyProcess.resize(term.cols, term.rows);
});

term.focus();
```

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on GitHub.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.