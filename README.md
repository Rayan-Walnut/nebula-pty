# Nebula PTY

Nebula PTY est un pseudo terminal développé en tant que module Node.js. Ce projet permet de créer et gérer des pseudo terminaux directement depuis une application Node.js.

## Installation

Pour installer Nebula PTY, utilisez npm :

```bash
npm install nebula-pty
```

## Utilisation

Voici un exemple simple de comment utiliser Nebula PTY dans votre projet :

```javascript
const NebulaPTY = require('nebula-pty');

// Créer une nouvelle instance de Nebula PTY
const pty = new NebulaPTY();

// Exécuter une commande dans le pseudo terminal
pty.run('ls -la');

// Écouter les données de sortie
pty.on('data', (data) => {
    console.log(data);
});

// Fermer le pseudo terminal
pty.close();
```

## API

### `NebulaPTY`

- `run(command: string)`: Exécute une commande dans le pseudo terminal.
- `on(event: string, callback: function)`: Écoute les événements du pseudo terminal (par exemple, 'data' pour les données de sortie).
- `close()`: Ferme le pseudo terminal.

## Contribuer

Les contributions sont les bienvenues ! Veuillez soumettre une pull request ou ouvrir une issue pour discuter des changements que vous souhaitez apporter.

## Licence

Ce projet est sous licence MIT. Voir le fichier [LICENSE](LICENSE) pour plus de détails.
