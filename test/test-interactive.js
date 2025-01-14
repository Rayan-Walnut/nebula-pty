const { WebTerminal } = require('../build/Release/terminal.node');
const readline = require('readline');

async function main() {
    const term = new WebTerminal();
    
    // Buffer pour accumuler les données
    let dataBuffer = '';
    
    term.onData((data) => {
        // Convertir le Buffer en string avec le bon encodage
        const text = data.toString('utf8');
        
        // Accumuler les données
        dataBuffer += text;
        
        // Chercher les lignes complètes
        const lines = dataBuffer.split('\r\n');
        if (lines.length > 1) {
            // Écrire toutes les lignes complètes
            lines.slice(0, -1).forEach(line => {
                process.stdout.write(line + '\r\n');
            });
            // Garder le reste dans le buffer
            dataBuffer = lines[lines.length - 1];
        }
    });

    // Démarrer le processus
    try {
        const pid = term.startProcess({ cols: 80, rfws: 24 });
        console.log('Terminal started with PID:', pid);
    } catch (error) {
        console.error('Failed to start terminal:', error);
        process.exit(1);
    }

    // Attendre l'initialisation
    await new Promise(resolve => setTimeout(resolve, 1000));

    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout,
        prompt: ''
    });

    // Mapper les commandes Unix vers les commandes Windows
    const commandMap = {
        'clear': 'cls',
        'ls': 'dir',
        'pwd': 'cd',
        // Ajoutez d'autres mappings si nécessaire
    };

    rl.on('line', (input) => {
        try {
            // Mapper la commande si nécessaire
            const mappedInput = commandMap[input.trim()] || input;
            term.write(mappedInput + '\r\n');
        } catch (error) {
            console.error('Write error:', error);
        }
    });

    // Gestion propre de la fermeture
    const cleanup = () => {
        console.log('\nClosing terminal...');
        rl.close();
        try {
            term.write('exit\r\n');
        } catch (error) {
            console.error('Error during cleanup:', error);
        }
        setTimeout(() => process.exit(0), 500);
    };

    process.on('SIGINT', cleanup);
    process.on('SIGTERM', cleanup);
}

main().catch(error => {
    console.error('Fatal error:', error);
    process.exit(1);
});