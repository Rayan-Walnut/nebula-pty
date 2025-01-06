const addon = require('./build/Release/test_addon.node');

class Terminal {
    constructor(options = {}) {
        this.terminal = new addon.Terminal();
        this.dataCallback = null;
        
        const pid = this.terminal.spawn(options.cols || 80, options.rows || 24);
        console.log('Terminal créé avec PID:', pid);

        // Écouter les données du terminal natif
        this.terminal.onData((data) => {
            if (this.dataCallback) {
                this.dataCallback(data);
            }
        });
    }

    write(data) {
        if (this.terminal) {
            this.terminal.write(data);
        }
    }

    onData(callback) {
        this.dataCallback = callback;
    }

    resize(cols, rows) {
        if (this.terminal) {
            this.terminal.resize(cols, rows);
        }
    }
}

module.exports = Terminal;