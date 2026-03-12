const vscode = require('vscode');
const path = require('path');

function activate(context) {
  let disposable = vscode.commands.registerCommand('nexo.runFile', async () => {
    const editor = vscode.window.activeTextEditor;

    // Check if there's an active editor
    if (!editor) {
      vscode.window.showErrorMessage('No file is open');
      return;
    }

    // Check if the file has .nx or .nxo extension
    const fileName = editor.document.fileName;
    if (!fileName.endsWith('.nx') && !fileName.endsWith('.nxo')) {
      vscode.window.showErrorMessage('This command only works for .nx or .nxo files');
      return;
    }

    // Save the file before running
    if (editor.document.isDirty) {
      await editor.document.save();
    }

    // Get the path to the compiler
    const compilerPath = path.join(path.dirname(context.extensionPath), 'compiler', 'nexo.exe');

    // Determine the shell being used
    const shellConfig = vscode.workspace.getConfiguration('terminal').get('integrated.defaultProfile.windows');
    const isPowerShell = shellConfig && (shellConfig.includes('PowerShell') || shellConfig.includes('pwsh'));

    // Prepare command based on shell type
    let command;
    if (isPowerShell) {
      command = `.\\"${compilerPath}\\" "\\${fileName}\\"`;
    } else {
      command = `"${compilerPath}" "${fileName}"`;
    }

    // Create or get the terminal
    let terminal = vscode.window.terminals.find(t => t.name === 'Nexo Compiler');
    if (!terminal) {
      terminal = vscode.window.createTerminal('Nexo Compiler');
    }

    // Show the terminal and run the command
    terminal.show();
    terminal.sendText(command);
  });

  context.subscriptions.push(disposable);
}

function deactivate() {}

module.exports = {
  activate,
  deactivate
};
