const { WebTerminal } = require('../build/Release/terminal.node');
const readline = require('readline');

async function main() {
    const term = new WebTerminal();
    
    console.log('Setting up data callback');
    term.onData((data) => {
        process.stdout.write(data.toString());
    });

    try {
        // Démarrer le terminal
        const pid = term.startProcess({ 
            cols: 120, 
            rows: 30 
        });
        console.log('Terminal started with PID:', pid);

        // Attendre l'initialisation
        await new Promise(resolve => setTimeout(resolve, 1000));

        // Configuration de readline
        const rl = readline.createInterface({
            input: process.stdin,
            output: process.stdout,
            prompt: ''
        });

        // Gestion des commandes
        rl.on('line', (input) => {
            try {
                if (input.trim()) {
                    term.write(input + '\r\n');
                    console.log('Command sent:', input);
                }
            } catch (error) {
                console.error('Error:', error);
            }
        });

        // Gestion de la fermeture
        const cleanup = () => {
            console.log('\nCleaning up...');
            rl.close();
            try {
                term.write('exit\r\n');
            } catch (error) {
                console.error('Error during cleanup:', error);
            }
            setTimeout(() => process.exit(0), 1000);
        };

        process.on('SIGINT', cleanup);
        process.on('SIGTERM', cleanup);

        // Test initial
        console.log('Sending test command...');
        term.write('dir\r\n');

    } catch (error) {
        console.error('Error:', error);
        process.exit(1);
    }
}

console.log('Starting terminal test...');
main().catch(error => {
    console.error('Fatal error:', error);
    process.exit(1);
});