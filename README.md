# Nebula PTY
Nebula PTY is a modern and efficient pseudo-terminal library for Windows. This project requires `mingw` and `cmake` to build and run. Additionally, you need to clone the `winpty` dependency into the `deps` directory.

## Requirements

- `mingw`
- `cmake`
- `nodejs`

## Installation

1. Clone the repository:
    ```sh
    git clone https://github.com/yourusername/nebula-pty.git
    cd nebula-pty
    ```

2. Clone the `winpty` dependency:
    ```sh
    git clone https://github.com/rprichard/winpty.git deps/winpty
    ```

3. Install Node.js dependencies:
    ```sh
    npm install
    ```

4. Build the project:
    ```sh
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ```

## Usage

Once the library is built, you can use it to create and manage pseudo-terminals in your applications. Here is a basic example of how to use Nebula PTY:

```cpp
#include <nebula-pty/nebula-pty.h>

int main() {
    // Initialize the PTY
    nebula::Pty pty;
    if (!pty.initialize()) {
        std::cerr << "Failed to initialize PTY" << std::endl;
        return 1;
    }

    // Create a new pseudo-terminal
    if (!pty.create()) {
        std::cerr << "Failed to create PTY" << std::endl;
        return 1;
    }

    // Write to the PTY
    const char* command = "dir\n";
    pty.write(command, strlen(command));

    // Read from the PTY
    char buffer[256];
    int bytesRead = pty.read(buffer, sizeof(buffer) - 1);
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        std::cout << "Output: " << buffer << std::endl;
    }

    // Cleanup
    pty.close();
    return 0;
}
```

## Author

This project is maintained by Rayan-Walnut. Contributions and feedback are welcome.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contributing

1. Fork the repository.
2. Create a new branch (`git checkout -b feature-branch`).
3. Make your changes.
4. Commit your changes (`git commit -am 'Add new feature'`).
5. Push to the branch (`git push origin feature-branch`).
6. Create a new Pull Request.

## Acknowledgements

- [winpty](https://github.com/rprichard/winpty) for providing the pseudo-terminal backend.
- The open-source community for their invaluable contributions and support.
## Usage

Once the library is built, you can use it to create and manage pseudo-terminals in your applications. Here is a basic example of how to use Nebula PTY with Node.js:

```javascript
const { spawn } = require('child_process');
const pty = require('nebula-pty');

const shell = process.env.ComSpec || 'cmd.exe';
const ptyProcess = pty.spawn(shell, [], {
    name: 'xterm-color',
    cols: 80,
    rows: 30,
    cwd: process.env.HOME,
    env: process.env
});

ptyProcess.on('data', function(data) {
    console.log(data);
});

ptyProcess.write('dir\r');
```