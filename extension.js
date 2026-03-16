const vscode = require('vscode');
const path = require('path');

function activate(context) {
  let disposable = vscode.commands.registerCommand('nibyo.runFile', async () => {
    const editor = vscode.window.activeTextEditor;

    // check if there's an active editor
    if (!editor) {
      vscode.window.showErrorMessage('No file is open');
      return;
    }

    // check if the file has .nb or .nb extension
    const fileName = editor.document.fileName;
    if (!fileName.endsWith('.nb') && !fileName.endsWith('.nb')) {
      vscode.window.showErrorMessage('This command only works for .nb or .nb files');
      return;
    }

    // save the file before running
    if (editor.document.isDirty) {
      await editor.document.save();
    }

    // get the path to the compiler
    const compilerPath = path.join(path.dirname(context.extensionPath), 'compiler', 'nibyo.exe');

    // determine the shell being used
    const shellConfig = vscode.workspace.getConfiguration('terminal').get('integrated.defaultProfile.windows');
    const isPowerShell = shellConfig && (shellConfig.includes('PowerShell') || shellConfig.includes('pwsh'));

    // prepare command based on shell type
    let command;
    if (isPowerShell) {
      command = `.\\"${compilerPath}\\" "\\${fileName}\\"`;
    } else {
      command = `"${compilerPath}" "${fileName}"`;
    }

    // create or get the terminal
    let terminal = vscode.window.terminals.find(t => t.name === 'Nibyo Compiler');
    if (!terminal) {
      terminal = vscode.window.createTerminal('Nibyo Compiler');
    }

    // show the terminal and run the command
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
