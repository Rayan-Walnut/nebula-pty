const { EventEmitter } = require('events');
const { WebTerminal } = require('../../build/Release/terminal');

class PTYWrapper extends EventEmitter {
    constructor(terminal) {
        super();
        this._terminal = terminal;

        // Configure le callback pour recevoir les données
        this._terminal.onData((data) => {
            this.emit('data', data);
        });
    }

    write(data) {
        this._terminal.write(data);
    }

    resize(cols, rows) {
        this._terminal.resize(cols, rows);
    }
}

function spawn(shell, args, options = {}) {
    const terminal = new WebTerminal();
    
    // Configurer et démarrer le processus
    terminal.startProcess({
        cols: options.cols || 80,
        rows: options.rows || 24,
        cwd: options.cwd || process.cwd(),
        env: options.env || process.env
    });

    // Créer et retourner l'interface compatible node-pty
    return new PTYWrapper(terminal);
}

module.exports = { spawn };