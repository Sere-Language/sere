"use strict";

const fs = require("fs");
const path = require("path");
const { spawn } = require("child_process");
const vscode = require("vscode");

const TOKEN_TYPES = [
  "namespace",
  "type",
  "class",
  "enum",
  "struct",
  "typeParameter",
  "parameter",
  "variable",
  "property",
  "enumMember",
  "function",
  "method",
  "macro",
  "keyword",
  "modifier",
  "string",
  "number",
  "regexp",
  "operator",
  "decorator",
];

const TOKEN_MODIFIERS = [
  "declaration",
  "definition",
  "readonly",
  "static",
  "abstract",
  "defaultLibrary",
];

function compilerName() {
  return process.platform === "win32" ? "sere.exe" : "sere";
}

function firstExisting(paths) {
  for (const candidate of paths) {
    if (candidate && fs.existsSync(candidate)) {
      return candidate;
    }
  }
  return "";
}

function hasPrelude(directory) {
  return Boolean(directory) && fs.existsSync(path.join(directory, "prelude.sere"));
}

function findProjectRoot(start) {
  let current = start;
  while (current) {
    if (fs.existsSync(path.join(current, "sere.toml"))) {
      return current;
    }
    const parent = path.dirname(current);
    if (parent === current) {
      break;
    }
    current = parent;
  }
  return "";
}

function findSere(workspaceFolder, forLsp) {
  const configured = vscode.workspace.getConfiguration("sere").get("compilerPath");
  let resolved = typeof configured === "string" ? configured : "";
  if (workspaceFolder && resolved.includes("${workspaceFolder}")) {
    resolved = resolved.replaceAll("${workspaceFolder}", workspaceFolder);
  }
  if (resolved.length > 0 && fs.existsSync(resolved)) {
    return resolved;
  }
  const name = compilerName();
  const venvBin = process.env.SERE_VENV_BIN;
  const projectRoot = workspaceFolder ? findProjectRoot(workspaceFolder) : "";
  const projectBins = [
    venvBin ? path.join(venvBin, name) : "",
    projectRoot ? path.join(projectRoot, "venv", "bin", name) : "",
    workspaceFolder ? path.join(workspaceFolder, "bin", name) : "",
  ];
  const buildBins = [
    workspaceFolder
      ? path.join(workspaceFolder, "build", "windows-clang-cl-relwithdebinfo", "bin", name)
      : "",
    workspaceFolder ? path.join(workspaceFolder, "build", "bin", name) : "",
  ];
  const order = forLsp ? buildBins.concat(projectBins) : projectBins.concat(buildBins);
  const found = firstExisting(order);
  return found || compilerName();
}

function findStdlib(workspaceFolder, compilerPath) {
  const configured = vscode.workspace.getConfiguration("sere").get("stdlibPath");
  if (typeof configured === "string" && hasPrelude(configured)) {
    return configured;
  }
  if (hasPrelude(process.env.SERE_STDLIB)) {
    return process.env.SERE_STDLIB;
  }
  const projectRoot = workspaceFolder ? findProjectRoot(workspaceFolder) : "";
  const compilerDir = compilerPath ? path.dirname(compilerPath) : "";
  return firstExisting([
    projectRoot && hasPrelude(path.join(projectRoot, "venv", "stdlib"))
      ? path.join(projectRoot, "venv", "stdlib")
      : "",
    workspaceFolder && hasPrelude(path.join(workspaceFolder, "stdlib"))
      ? path.join(workspaceFolder, "stdlib")
      : "",
    compilerDir && hasPrelude(path.join(compilerDir, "stdlib"))
      ? path.join(compilerDir, "stdlib")
      : "",
  ]);
}

function isContextDocument(document, session) {
  if (!document) {
    return false;
  }
  const fileName = path.basename(document.uri.fsPath);
  if (fileName === "sere.toml") {
    return true;
  }
  if (document.languageId !== "sere") {
    return false;
  }
  const filePath = document.uri.fsPath;
  const stdlib = session ? session.resolvedPaths().stdlib : "";
  const folder = session ? session.workspaceFolder() : "";
  const roots = [
    stdlib,
    folder ? path.join(folder, "stdlib") : "",
    folder ? path.join(folder, "venv", "stdlib") : "",
    folder ? path.join(folder, "libs") : "",
    folder ? path.join(folder, "src") : "",
  ].filter(Boolean);
  return roots.some((root) => filePath === root || filePath.startsWith(root + path.sep));
}

function compilerEnv(workspaceFolder, compilerPath) {
  const env = { ...process.env };
  const stdlib = findStdlib(workspaceFolder, compilerPath);
  if (stdlib.length > 0) {
    env.SERE_STDLIB = stdlib;
  }
  return env;
}

function runSereCommand(session, args, title, forLsp) {
  const workspaceFolder = session.workspaceFolder();
  const sere = findSere(workspaceFolder, Boolean(forLsp));
  const cwd = workspaceFolder ? findProjectRoot(workspaceFolder) || workspaceFolder : undefined;
  const child = spawn(sere, args, { cwd, env: compilerEnv(workspaceFolder, sere) });
  let stderr = "";
  child.stderr.on("data", (chunk) => {
    stderr += chunk.toString();
  });
  child.on("exit", (code) => {
    if (code === 0) {
      vscode.window.setStatusBarMessage("Sere: " + title, 3000);
      return;
    }
    vscode.window.showErrorMessage(stderr.trim() || "Sere " + title + " failed.");
  });
}

class LspClient {
  constructor(child) {
    this.child = child;
    this.buffer = Buffer.alloc(0);
    this.nextId = 1;
    this.pending = new Map();
    this.onNotification = () => {};
    child.stdout.on("data", (chunk) => this.feed(chunk));
    child.stderr.on("data", (chunk) => console.error(chunk.toString()));
  }

  feed(chunk) {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    while (true) {
      const headerEnd = this.buffer.indexOf("\r\n\r\n");
      if (headerEnd < 0) {
        return;
      }
      const header = this.buffer.slice(0, headerEnd).toString("utf8");
      const match = /Content-Length:\s*(\d+)/i.exec(header);
      if (!match) {
        this.buffer = this.buffer.slice(headerEnd + 4);
        continue;
      }
      const length = Number(match[1]);
      const bodyStart = headerEnd + 4;
      if (this.buffer.length < bodyStart + length) {
        return;
      }
      const body = this.buffer.slice(bodyStart, bodyStart + length).toString("utf8");
      this.buffer = this.buffer.slice(bodyStart + length);
      try {
        this.dispatch(JSON.parse(body));
      } catch (error) {
        console.error("Sere LSP: invalid JSON", error);
      }
    }
  }

  dispatch(message) {
    if (Object.prototype.hasOwnProperty.call(message, "id") && this.pending.has(message.id)) {
      const { resolve, reject } = this.pending.get(message.id);
      this.pending.delete(message.id);
      if (message.error) {
        reject(new Error(message.error.message || "LSP error"));
        return;
      }
      resolve(message.result);
      return;
    }
    if (typeof message.method === "string") {
      this.onNotification(message.method, message.params || {});
    }
  }

  send(payload) {
    const body = JSON.stringify(payload);
    this.child.stdin.write(`Content-Length: ${Buffer.byteLength(body)}\r\n\r\n${body}`);
  }

  request(method, params) {
    const id = this.nextId;
    this.nextId += 1;
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      this.send({ jsonrpc: "2.0", id, method, params });
    });
  }

  notify(method, params) {
    this.send({ jsonrpc: "2.0", method, params });
  }

  async stop() {
    try {
      await this.request("shutdown", null);
      this.notify("exit", {});
    } catch (error) {
      console.error(error);
    }
    try {
      this.child.kill();
    } catch (error) {
      console.error(error);
    }
  }
}

function toPosition(position) {
  return { line: position.line, character: position.character };
}

function fromRange(range) {
  return new vscode.Range(
    range.start.line,
    range.start.character,
    range.end.line,
    range.end.character,
  );
}

function fromLocation(item) {
  return new vscode.Location(vscode.Uri.parse(item.uri), fromRange(item.range));
}

function fromLocations(result) {
  const locations = Array.isArray(result) ? result : result ? [result] : [];
  return locations.filter((item) => item && item.uri && item.range).map((item) => fromLocation(item));
}

function fromWorkspaceEdit(result) {
  const edit = new vscode.WorkspaceEdit();
  if (!result || typeof result.changes !== "object") {
    return edit;
  }
  for (const [uri, edits] of Object.entries(result.changes)) {
    if (!Array.isArray(edits)) {
      continue;
    }
    for (const item of edits) {
      edit.replace(vscode.Uri.parse(uri), fromRange(item.range), item.newText);
    }
  }
  return edit;
}

function toSymbol(item) {
  const symbol = new vscode.DocumentSymbol(
    item.name,
    item.detail || "",
    item.kind,
    fromRange(item.range),
    fromRange(item.selectionRange || item.range),
  );
  if (Array.isArray(item.children)) {
    symbol.children = item.children.map((child) => toSymbol(child));
  }
  return symbol;
}

function toCompletion(item) {
  const completion = new vscode.CompletionItem(item.label, item.kind);
  completion.detail = item.detail;
  if (item.documentation) {
    completion.documentation = new vscode.MarkdownString(item.documentation);
  }
  if (item.insertText) {
    completion.insertText =
      item.insertTextFormat === 2 ? new vscode.SnippetString(item.insertText) : item.insertText;
  }
  if (item.sortText) {
    completion.sortText = item.sortText;
  }
  if (item.filterText) {
    completion.filterText = item.filterText;
  }
  return completion;
}

function documentPosition(document, position) {
  return { textDocument: { uri: document.uri.toString() }, position: toPosition(position) };
}

const DOCUMENT_DEBOUNCE_MS = 80;
const CONTEXT_DEBOUNCE_MS = 120;
const FILE_CHANGE_CREATED = 1;
const FILE_CHANGE_CHANGED = 2;
const FILE_CHANGE_DELETED = 3;

class SereLanguageClient {
  constructor(context) {
    this.context = context;
    this.client = null;
    this.child = null;
    this.stopping = false;
    this.changeTimers = new Map();
    this.watchers = [];
    this.pendingWatched = new Map();
    this.watchedTimer = null;
    this.startedCompiler = "";
    this.startedStdlib = "";
    this.semanticTokensChanged = new vscode.EventEmitter();
    this.inlayHintsChanged = new vscode.EventEmitter();
    this.codeLensesChanged = new vscode.EventEmitter();
    this.diagnostics = vscode.languages.createDiagnosticCollection("sere");
    this.status = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    this.status.command = "sere.restartLanguageServer";
    this.status.text = "Sere";
    this.status.tooltip = "Sere language server — click to restart";
    this.status.show();
    context.subscriptions.push(
      this.diagnostics,
      this.status,
      this.semanticTokensChanged,
      this.inlayHintsChanged,
      this.codeLensesChanged,
    );
  }

  workspaceFolder() {
    return vscode.workspace.workspaceFolders
      ? vscode.workspace.workspaceFolders[0].uri.fsPath
      : undefined;
  }

  resolvedPaths() {
    const workspaceFolder = this.workspaceFolder();
    const compiler = findSere(workspaceFolder, true);
    return {
      workspaceFolder,
      compiler,
      stdlib: findStdlib(workspaceFolder, compiler),
    };
  }

  disposeWatchers() {
    for (const watcher of this.watchers) {
      watcher.dispose();
    }
    this.watchers = [];
    if (this.watchedTimer) {
      clearTimeout(this.watchedTimer);
      this.watchedTimer = null;
    }
    this.pendingWatched.clear();
  }

  contextWatchPatterns() {
    const { workspaceFolder, stdlib } = this.resolvedPaths();
    const patterns = [];
    const add = (base, pattern) => {
      if (base) {
        patterns.push(new vscode.RelativePattern(base, pattern));
      }
    };
    add(stdlib, "**/*.sere");
    add(workspaceFolder, "**/sere.toml");
    add(workspaceFolder, "**/*.sere");
    add(workspaceFolder, "**/*.slib");
    return patterns;
  }

  setupContextWatchers() {
    this.disposeWatchers();
    for (const pattern of this.contextWatchPatterns()) {
      const watcher = vscode.workspace.createFileSystemWatcher(pattern);
      watcher.onDidCreate((uri) => this.queueWatchedChange(uri, FILE_CHANGE_CREATED));
      watcher.onDidChange((uri) => this.queueWatchedChange(uri, FILE_CHANGE_CHANGED));
      watcher.onDidDelete((uri) => this.queueWatchedChange(uri, FILE_CHANGE_DELETED));
      this.watchers.push(watcher);
      this.context.subscriptions.push(watcher);
    }
  }

  queueWatchedChange(uri, type) {
    this.pendingWatched.set(uri.toString(), { uri: uri.toString(), type });
    if (this.watchedTimer) {
      clearTimeout(this.watchedTimer);
    }
    this.watchedTimer = setTimeout(() => {
      this.watchedTimer = null;
      this.flushWatchedChanges();
    }, CONTEXT_DEBOUNCE_MS);
  }

  flushWatchedChanges() {
    if (this.pendingWatched.size === 0) {
      return;
    }
    const changes = Array.from(this.pendingWatched.values());
    this.pendingWatched.clear();
    this.notify("workspace/didChangeWatchedFiles", { changes });
    this.refreshEditorProviders();
  }

  refreshEditorProviders() {
    this.semanticTokensChanged.fire();
    this.inlayHintsChanged.fire();
    this.codeLensesChanged.fire();
  }

  notifyConfiguration() {
    const { compiler, stdlib } = this.resolvedPaths();
    this.notify("workspace/didChangeConfiguration", {
      settings: {
        sere: {
          compilerPath: compiler,
          stdlibPath: stdlib,
        },
      },
    });
    this.refreshEditorProviders();
  }

  async onSettingsChanged() {
    const { compiler, stdlib } = this.resolvedPaths();
    const compilerChanged = compiler !== this.startedCompiler;
    const stdlibChanged = stdlib !== this.startedStdlib;
    if (compilerChanged || stdlibChanged) {
      await this.restart();
      return;
    }
    this.notifyConfiguration();
  }

  start() {
    this.stopping = false;
    const { workspaceFolder, compiler, stdlib } = this.resolvedPaths();
    this.startedCompiler = compiler;
    this.startedStdlib = stdlib;
    this.status.text = "Sere";
    this.status.tooltip = "Sere language server: " + compiler;
    if (stdlib) {
      this.status.tooltip += "\nstdlib: " + stdlib;
    }
    this.child = spawn(compiler, ["--lsp"], {
      stdio: ["pipe", "pipe", "pipe"],
      env: compilerEnv(workspaceFolder, compiler),
    });
    this.child.on("error", (error) => {
      this.status.text = "Sere $(error)";
      vscode.window.showErrorMessage(`Sere language server failed to start: ${error.message}`);
    });
    this.child.on("exit", (code) => {
      if (!this.stopping && code !== 0 && code !== null) {
        this.status.text = "Sere $(error)";
        vscode.window.showWarningMessage(`Sere language server exited (${code}). Use Sere: Restart Language Server.`);
      }
    });
    this.client = new LspClient(this.child);
    this.client.onNotification = (method, params) => {
      if (method !== "textDocument/publishDiagnostics") {
        return;
      }
      const uri = vscode.Uri.parse(params.uri);
      const items = (params.diagnostics || []).map((item) => {
        const severity =
          item.severity === 2
            ? vscode.DiagnosticSeverity.Warning
            : item.severity === 3
              ? vscode.DiagnosticSeverity.Information
              : vscode.DiagnosticSeverity.Error;
        const diagnostic = new vscode.Diagnostic(fromRange(item.range), item.message, severity);
        if (item.code) {
          diagnostic.code = item.code;
        }
        diagnostic.source = item.source || "sere";
        return diagnostic;
      });
      this.diagnostics.set(uri, items);
    };
    this.client
      .request("initialize", {
        processId: process.pid,
        rootUri: workspaceFolder ? vscode.Uri.file(workspaceFolder).toString() : null,
        initializationOptions: {
          compilerPath: compiler,
          stdlibPath: stdlib,
        },
        capabilities: {
          workspace: {
            workspaceFolders: true,
            didChangeConfiguration: { dynamicRegistration: false },
            didChangeWatchedFiles: { dynamicRegistration: false },
          },
          textDocument: {
            hover: { contentFormat: ["markdown"] },
            completion: { completionItem: { snippetSupport: true } },
            publishDiagnostics: { relatedInformation: false },
          },
        },
      })
      .then(() => {
        this.status.text = "Sere";
        this.client.notify("initialized", {});
        this.setupContextWatchers();
        for (const document of vscode.workspace.textDocuments) {
          this.openDocument(document);
        }
      })
      .catch((error) => {
        this.status.text = "Sere $(error)";
        vscode.window.showErrorMessage(`Sere language server initialize failed: ${error.message}`);
      });
  }

  async stop() {
    this.stopping = true;
    this.disposeWatchers();
    if (this.client !== null) {
      await this.client.stop();
      this.client = null;
    }
    this.child = null;
    this.diagnostics.clear();
  }

  async restart() {
    await this.stop();
    this.start();
    vscode.window.setStatusBarMessage("Sere language server restarted", 2500);
  }

  request(method, params) {
    if (this.client === null) {
      return Promise.resolve(null);
    }
    return this.client.request(method, params).catch((error) => {
      console.error(method, error);
      return null;
    });
  }

  notify(method, params) {
    if (this.client !== null) {
      this.client.notify(method, params);
    }
  }

  openDocument(document) {
    if (document.languageId !== "sere") {
      return;
    }
    this.notify("textDocument/didOpen", {
      textDocument: {
        uri: document.uri.toString(),
        languageId: "sere",
        version: document.version,
        text: document.getText(),
      },
    });
  }
}

function activate(context) {
  const session = new SereLanguageClient(context);
  session.start();

  const compileCurrentFile = () => {
    const editor = vscode.window.activeTextEditor;
    if (editor === undefined || editor.document.languageId !== "sere") {
      vscode.window.showErrorMessage("Open a .sere file to compile.");
      return;
    }
    const workspaceFolder = session.workspaceFolder();
    const sere = findSere(workspaceFolder, false);
    const child = spawn(sere, [editor.document.uri.fsPath], {
      env: compilerEnv(workspaceFolder, sere),
    });
    let stderr = "";
    child.stderr.on("data", (chunk) => {
      stderr += chunk.toString();
    });
    child.on("exit", (code) => {
      if (code === 0) {
        vscode.window.setStatusBarMessage("Sere: compiled " + path.basename(editor.document.fileName), 3000);
        return;
      }
      vscode.window.showErrorMessage(stderr.trim() || "Sere compile failed.");
    });
  };

  const locationProvider = (method) => ({
    provideDefinition(document, position) {
      return session.request(method, documentPosition(document, position)).then(fromLocations);
    },
  });

  context.subscriptions.push(
    vscode.commands.registerCommand("sere.restartLanguageServer", () => session.restart()),
    vscode.commands.registerCommand("sere.compileCurrentFile", () => compileCurrentFile()),
    vscode.commands.registerCommand("sere.buildProject", () =>
      runSereCommand(session, ["build"], "built project"),
    ),
    vscode.commands.registerCommand("sere.runProject", () =>
      runSereCommand(session, ["run"], "ran project"),
    ),
    vscode.commands.registerCommand("sere.refreshBin", () => {
      runSereCommand(session, ["refresh-bin"], "refreshed ./bin", true);
      session.notifyConfiguration();
    }),
    vscode.commands.registerCommand("sere.openSettings", () =>
      vscode.commands.executeCommand("workbench.action.openSettings", "@ext:sere.sere"),
    ),
    vscode.workspace.onDidOpenTextDocument((document) => session.openDocument(document)),
    vscode.workspace.onDidChangeTextDocument((event) => {
      if (event.document.languageId !== "sere") {
        return;
      }
      const uri = event.document.uri.toString();
      const previous = session.changeTimers.get(uri);
      if (previous) {
        clearTimeout(previous);
      }
      const timer = setTimeout(() => {
        session.changeTimers.delete(uri);
        session.notify("textDocument/didChange", {
          textDocument: { uri, version: event.document.version },
          contentChanges: [{ text: event.document.getText() }],
        });
        if (isContextDocument(event.document, session)) {
          session.refreshEditorProviders();
        }
      }, DOCUMENT_DEBOUNCE_MS);
      session.changeTimers.set(uri, timer);
    }),
    vscode.workspace.onDidSaveTextDocument((document) => {
      if (isContextDocument(document, session) || document.fileName.endsWith("sere.toml")) {
        session.queueWatchedChange(document.uri, FILE_CHANGE_CHANGED);
      }
    }),
    vscode.workspace.onDidChangeConfiguration((event) => {
      if (
        event.affectsConfiguration("sere.compilerPath") ||
        event.affectsConfiguration("sere.stdlibPath")
      ) {
        session.onSettingsChanged();
        return;
      }
      if (event.affectsConfiguration("sere")) {
        session.refreshEditorProviders();
      }
    }),
    vscode.workspace.onDidChangeWorkspaceFolders(() => {
      session.restart();
    }),
    vscode.workspace.onDidCloseTextDocument((document) => {
      if (document.languageId !== "sere") {
        return;
      }
      session.notify("textDocument/didClose", {
        textDocument: { uri: document.uri.toString() },
      });
    }),
    vscode.languages.registerDefinitionProvider("sere", locationProvider("textDocument/definition")),
    vscode.languages.registerTypeDefinitionProvider("sere", {
      provideTypeDefinition(document, position) {
        return session
          .request("textDocument/typeDefinition", documentPosition(document, position))
          .then(fromLocations);
      },
    }),
    vscode.languages.registerImplementationProvider("sere", {
      provideImplementation(document, position) {
        return session
          .request("textDocument/implementation", documentPosition(document, position))
          .then(fromLocations);
      },
    }),
    vscode.languages.registerReferenceProvider("sere", {
      provideReferences(document, position) {
        return session
          .request("textDocument/references", {
            ...documentPosition(document, position),
            context: { includeDeclaration: true },
          })
          .then(fromLocations);
      },
    }),
    vscode.languages.registerDocumentHighlightProvider("sere", {
      provideDocumentHighlights(document, position) {
        return session
          .request("textDocument/documentHighlight", documentPosition(document, position))
          .then((result) => {
            const items = Array.isArray(result) ? result : [];
            return items.map(
              (item) => new vscode.DocumentHighlight(fromRange(item.range), item.kind || 1),
            );
          });
      },
    }),
    vscode.languages.registerRenameProvider("sere", {
      prepareRename(document, position) {
        return session
          .request("textDocument/prepareRename", documentPosition(document, position))
          .then((result) => {
            if (!result) {
              throw new Error("The current symbol cannot be renamed.");
            }
            return fromRange(result);
          });
      },
      provideRenameEdits(document, position, newName) {
        return session
          .request("textDocument/rename", { ...documentPosition(document, position), newName })
          .then(fromWorkspaceEdit);
      },
    }),
    vscode.languages.registerCodeLensProvider("sere", {
      onDidChangeCodeLenses: session.codeLensesChanged.event,
      provideCodeLenses(document) {
        if (!vscode.workspace.getConfiguration("sere").get("codeLens")) {
          return [];
        }
        return session
          .request("textDocument/codeLens", { textDocument: { uri: document.uri.toString() } })
          .then((result) => {
            const items = Array.isArray(result) ? result : [];
            return items.map((item) => {
              const lens = new vscode.CodeLens(fromRange(item.range));
              if (item.command) {
                const locations = Array.isArray(item.command.arguments)
                  ? item.command.arguments[2] || []
                  : [];
                lens.command = {
                  title: item.command.title,
                  command: "editor.action.showReferences",
                  arguments: [
                    document.uri,
                    fromRange(item.range).start,
                    locations.map((location) => fromLocation(location)),
                  ],
                };
              }
              return lens;
            });
          });
      },
    }),
    vscode.languages.registerDocumentSymbolProvider("sere", {
      provideDocumentSymbols(document) {
        return session
          .request("textDocument/documentSymbol", { textDocument: { uri: document.uri.toString() } })
          .then((result) => {
            const items = Array.isArray(result) ? result : [];
            return items.map((item) => toSymbol(item));
          });
      },
    }),
    vscode.languages.registerWorkspaceSymbolProvider({
      provideWorkspaceSymbols(query) {
        return session.request("workspace/symbol", { query }).then((result) => {
          const items = Array.isArray(result) ? result : [];
          return items
            .filter((item) => item && item.location)
            .map(
              (item) =>
                new vscode.SymbolInformation(
                  item.name,
                  item.kind,
                  item.containerName || "",
                  fromLocation(item.location),
                ),
            );
        });
      },
    }),
    vscode.languages.registerSignatureHelpProvider(
      "sere",
      {
        provideSignatureHelp(document, position) {
          return session
            .request("textDocument/signatureHelp", documentPosition(document, position))
            .then((result) => {
              if (!result || !Array.isArray(result.signatures) || result.signatures.length === 0) {
                return undefined;
              }
              const help = new vscode.SignatureHelp();
              help.signatures = result.signatures.map((signature) => {
                const info = new vscode.SignatureInformation(signature.label || "");
                if (signature.documentation) {
                  info.documentation = new vscode.MarkdownString(String(signature.documentation));
                }
                if (Array.isArray(signature.parameters)) {
                  info.parameters = signature.parameters.map((parameter) => {
                    if (Array.isArray(parameter.label) && parameter.label.length === 2) {
                      return new vscode.ParameterInformation(
                        [Number(parameter.label[0]), Number(parameter.label[1])],
                        parameter.documentation || "",
                      );
                    }
                    const label =
                      typeof parameter.label === "string" ? parameter.label : signature.label;
                    return new vscode.ParameterInformation(label, parameter.documentation || "");
                  });
                }
                return info;
              });
              help.activeSignature =
                typeof result.activeSignature === "number" ? result.activeSignature : 0;
              help.activeParameter =
                typeof result.activeParameter === "number" ? result.activeParameter : 0;
              return help;
            });
        },
      },
      {
        triggerCharacters: ["(", ",", "!"],
        retriggerCharacters: [",", " "],
      },
    ),
    vscode.languages.registerInlayHintsProvider("sere", {
      onDidChangeInlayHints: session.inlayHintsChanged.event,
      provideInlayHints(document, range) {
        return session
          .request("textDocument/inlayHint", {
            textDocument: { uri: document.uri.toString() },
            range: { start: toPosition(range.start), end: toPosition(range.end) },
          })
          .then((result) => {
            const items = Array.isArray(result) ? result : [];
            return items.map((item) => {
              const hint = new vscode.InlayHint(
                new vscode.Position(item.position.line, item.position.character),
                item.label,
                item.kind === 1 ? vscode.InlayHintKind.Type : vscode.InlayHintKind.Parameter,
              );
              hint.paddingLeft = Boolean(item.paddingLeft);
              hint.paddingRight = Boolean(item.paddingRight);
              return hint;
            });
          });
      },
    }),
    vscode.languages.registerHoverProvider("sere", {
      provideHover(document, position) {
        return session
          .request("textDocument/hover", documentPosition(document, position))
          .then((result) => {
            if (!result || !result.contents) {
              return undefined;
            }
            const value =
              typeof result.contents === "string" ? result.contents : result.contents.value;
            const markdown = new vscode.MarkdownString(value, true);
            markdown.supportHtml = false;
            return new vscode.Hover(markdown, result.range ? fromRange(result.range) : undefined);
          });
      },
    }),
    vscode.languages.registerCompletionItemProvider(
      "sere",
      {
        provideCompletionItems(document, position) {
          return session
            .request("textDocument/completion", documentPosition(document, position))
            .then((result) => {
              const items = Array.isArray(result) ? result : [];
              return items.map((item) => toCompletion(item));
            });
        },
      },
      ".",
      '"',
      "@",
      "!",
    ),
    vscode.languages.registerFoldingRangeProvider("sere", {
      provideFoldingRanges(document) {
        return session
          .request("textDocument/foldingRange", { textDocument: { uri: document.uri.toString() } })
          .then((result) => {
            const items = Array.isArray(result) ? result : [];
            return items.map(
              (item) =>
                new vscode.FoldingRange(item.startLine, item.endLine, vscode.FoldingRangeKind.Region),
            );
          });
      },
    }),
    vscode.languages.registerDocumentFormattingEditProvider("sere", {
      provideDocumentFormattingEdits(document) {
        return session
          .request("textDocument/formatting", {
            textDocument: { uri: document.uri.toString() },
            options: { tabSize: 4, insertSpaces: true },
          })
          .then((result) => {
            const items = Array.isArray(result) ? result : [];
            return items.map((item) => vscode.TextEdit.replace(fromRange(item.range), item.newText));
          });
      },
    }),
    vscode.languages.registerDocumentRangeFormattingEditProvider("sere", {
      provideDocumentRangeFormattingEdits(document, range) {
        return session
          .request("textDocument/rangeFormatting", {
            textDocument: { uri: document.uri.toString() },
            range: { start: toPosition(range.start), end: toPosition(range.end) },
            options: { tabSize: 4, insertSpaces: true },
          })
          .then((result) => {
            const items = Array.isArray(result) ? result : [];
            return items.map((item) => vscode.TextEdit.replace(fromRange(item.range), item.newText));
          });
      },
    }),
    vscode.languages.registerCodeActionsProvider("sere", {
      provideCodeActions(document, range) {
        return session
          .request("textDocument/codeAction", {
            textDocument: { uri: document.uri.toString() },
            range: { start: toPosition(range.start), end: toPosition(range.end) },
            context: { diagnostics: [] },
          })
          .then((result) => (Array.isArray(result) ? result : []));
      },
    }),
    vscode.languages.registerDocumentSemanticTokensProvider(
      "sere",
      {
        onDidChangeSemanticTokens: session.semanticTokensChanged.event,
        provideDocumentSemanticTokens(document) {
          return session
            .request("textDocument/semanticTokens/full", {
              textDocument: { uri: document.uri.toString() },
            })
            .then((result) => {
              const data = result && Array.isArray(result.data) ? result.data : [];
              return new vscode.SemanticTokens(new Uint32Array(data));
            });
        },
      },
      new vscode.SemanticTokensLegend(TOKEN_TYPES, TOKEN_MODIFIERS),
    ),
    { dispose: () => session.stop() },
  );
}

function deactivate() {}

module.exports = { activate, deactivate };
