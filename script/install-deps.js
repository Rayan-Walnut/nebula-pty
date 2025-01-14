const { execSync } = require('child_process');
const os = require('os');

function installDeps() {
    try {
        console.log('Installing required Python packages...');
        
        // Installation de setuptools qui inclut distutils
        execSync('python -m pip install --upgrade pip setuptools wheel', { stdio: 'inherit' });
        
        // Vérification de node-gyp
        try {
            execSync('node-gyp --version');
        } catch (e) {
            console.log('Installing node-gyp...');
            execSync('npm install -g node-gyp', { stdio: 'inherit' });
        }

        console.log('All dependencies installed successfully!');
    } catch (error) {
        console.error('Error installing dependencies:', error);
        process.exit(1);
    }
}

installDeps();