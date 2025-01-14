const { WebTerminal } = require('./build/Release/terminal.node');
const { Terminal } = require('@xterm/xterm');
const { FitAddon } = require('@xterm/addon-fit');
const { WebLinksAddon } = require('@xterm/addon-web-links');

class NebulaPTY {
    constructor(options = {}) {
        this.terminal = new Terminal({
            cursorBlink: true,
            cursorStyle: 'block',
            fontFamily: 'Consolas, monospace',
            theme: {
                background: '#1e1e1e',
                foreground: '#d4d4d4',
                cursor: '#ffffff'
            },
            ...options
        });

        this.ptyProcess = new WebTerminal();
        this.fitAddon = new FitAddon();
        this.webLinksAddon = new WebLinksAddon();
    }

    open(container) {
        if (typeof container === 'string') {
            container = document.getElementById(container);
        }

        this.terminal.loadAddon(this.fitAddon);
        this.terminal.loadAddon(this.webLinksAddon);
        this.terminal.open(container);
        this.fitAddon.fit();

        this.ptyProcess.startProcess({
            cols: this.terminal.cols,
            rows: this.terminal.rows,
            env: process.env
        });

        this.terminal.onData(data => this.ptyProcess.write(data));
        this.ptyProcess.onData(data => this.terminal.write(data));

        window.addEventListener('resize', () => {
            this.fitAddon.fit();
            this.ptyProcess.resize(this.terminal.cols, this.terminal.rows);
        });

        this.terminal.focus();
    }

    write(data) {
        this.ptyProcess.write(data);
    }

    onData(callback) {
        this.terminal.onData(callback);
    }

    resize(cols, rows) {
        this.ptyProcess.resize(cols, rows);
    }

    focus() {
        this.terminal.focus();
    }

    clear() {
        this.terminal.clear();
    }
}

module.exports = {
    NebulaPTY,
    WebTerminal
};s