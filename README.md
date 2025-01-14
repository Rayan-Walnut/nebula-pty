# Nebula PTY

A modern terminal emulator for Electron applications with native PTY support.

## Installation

```bash
npm install nebula-pty
```

## Usage

1. Add required HTML:
```html
<!DOCTYPE html>
<html>
<head>
    <link rel="stylesheet" href="node_modules/@xterm/xterm/css/xterm.css" />
</head>
<body>
    <div id="terminal-container"></div>
</body>
</html>
```

2. Use in your JavaScript:
```javascript
const { NebulaPTY } = require('nebula-pty');

const terminal = new NebulaPTY({
    fontSize: 14,
    theme: {
        background: '#1e1e1e',
        foreground: '#d4d4d4'
    }
});

terminal.open('terminal-container');
```

## API

### NebulaPTY
- `constructor(options)`: Create a new terminal instance
- `open(container)`: Open terminal in container (ID or DOM element)
- `write(data)`: Write data to terminal
- `onData(callback)`: Listen for user input
- `resize(cols, rows)`: Resize terminal
- `focus()`: Focus terminal
- `clear()`: Clear terminal

## License
ISC