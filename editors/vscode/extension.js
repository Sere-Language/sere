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

let activeCompilerContext = null;

function isAbsoluteCompiler(compilerPath) {
  return Boolean(compilerPath) && compilerPath !== compilerName() && fs.existsSync(compilerPath);
}

function sameCompiler(left, right) {
  if (!left || !right) {
    return false;
  }
  return path.resolve(left) === path.resolve(right);
}

function queryCompilerContext(compilerPath) {
  return new Promise((resolve) => {
    if (!isAbsoluteCompiler(compilerPath)) {
      resolve(null);
      return;
    }
    const env = { ...process.env };
    delete env.SERE_STDLIB;
    const child = spawn(compilerPath, ["--print-env"], { env });
    let stdout = "";
    const timer = setTimeout(() => {
      child.kill();
      resolve(null);
    }, 8000);
    child.stdout.on("data", (chunk) => {
      stdout += chunk.toString();
    });
    child.on("error", () => {
      clearTimeout(timer);
      resolve(null);
    });
    child.on("exit", () => {
      clearTimeout(timer);
      try {
        const parsed = JSON.parse(stdout.trim());
        if (parsed && typeof parsed.stdlib === "string") {
          resolve(parsed);
          return;
        }
      } catch (_error) {
        // Older compilers do not implement --print-env.
      }
      resolve(null);
    });
  });
}

function contextFor(compilerPath) {
  if (activeCompilerContext && sameCompiler(activeCompilerContext.compiler, compilerPath)) {
    return activeCompilerContext;
  }
  return null;
}

function stdlibFromCompiler(compilerPath) {
  if (!isAbsoluteCompiler(compilerPath)) {
    return "";
  }
  let directory = path.dirname(path.resolve(compilerPath));
  for (let depth = 0; depth < 4; depth += 1) {
    const stdlib = path.join(directory, "stdlib");
    if (hasPrelude(stdlib)) {
      return stdlib;
    }
    const parent = path.dirname(directory);
    if (parent === directory) {
      break;
    }
    directory = parent;
  }
  return "";
}

function findStdlib(workspaceFolder, compilerPath) {
  const configured = vscode.workspace.getConfiguration("sere").get("stdlibPath");
  if (typeof configured === "string" && hasPrelude(configured)) {
    return configured;
  }
  const workspaceStdlib =
    workspaceFolder && hasPrelude(path.join(workspaceFolder, "stdlib"))
      ? path.join(workspaceFolder, "stdlib")
      : "";
  if (workspaceStdlib) {
    return workspaceStdlib;
  }
  const reported = contextFor(compilerPath);
  if (reported && hasPrelude(reported.stdlib)) {
    return reported.stdlib;
  }
  const fromCompiler = stdlibFromCompiler(compilerPath);
  if (fromCompiler) {
    return fromCompiler;
  }
  const projectRoot = workspaceFolder ? findProjectRoot(workspaceFolder) : "";
  return firstExisting([
    hasPrelude(process.env.SERE_STDLIB) ? process.env.SERE_STDLIB : "",
    projectRoot && hasPrelude(path.join(projectRoot, "venv", "stdlib"))
      ? path.join(projectRoot, "venv", "stdlib")
      : "",
  ]);
}

function settingsTarget() {
  return vscode.workspace.workspaceFolders
    ? vscode.ConfigurationTarget.Workspace
    : vscode.ConfigurationTarget.Global;
}

function discoveredCompilers(workspaceFolder) {
  const name = compilerName();
  const projectRoot = workspaceFolder ? findProjectRoot(workspaceFolder) : "";
  const configured = vscode.workspace.getConfiguration("sere").get("compilerPath");
  let resolved = typeof configured === "string" ? configured : "";
  if (workspaceFolder && resolved.includes("${workspaceFolder}")) {
    resolved = resolved.replaceAll("${workspaceFolder}", workspaceFolder);
  }
  return [
    resolved,
    workspaceFolder ? path.join(workspaceFolder, "bin", name) : "",
    projectRoot ? path.join(projectRoot, "venv", "bin", name) : "",
    process.env.SERE_VENV_BIN ? path.join(process.env.SERE_VENV_BIN, name) : "",
    workspaceFolder
      ? path.join(workspaceFolder, "build", "windows-clang-cl-relwithdebinfo", "bin", name)
      : "",
    workspaceFolder
      ? path.join(workspaceFolder, "build", "windows-clang-cl-relwithdebinfo", "bin", "staging", name)
      : "",
    workspaceFolder ? path.join(workspaceFolder, "build", "bin", name) : "",
  ].filter((candidate, index, all) => {
    return candidate && fs.existsSync(candidate) && all.indexOf(candidate) === index;
  });
}

async function applyActiveCompiler(compilerPath, session) {
  const config = vscode.workspace.getConfiguration("sere");
  await config.update("compilerPath", compilerPath, settingsTarget());
  const context = await queryCompilerContext(compilerPath);
  activeCompilerContext = context;
  const stdlib = (context && context.stdlib) || stdlibFromCompiler(compilerPath);
  if (session) {
    await session.restart();
  }
  const version = context && context.version ? "sere " + context.version + " · " : "";
  const stdlibLabel = stdlib || "(compiler did not report a stdlib)";
  vscode.window.setStatusBarMessage(
    "Sere compiler: " + version + compilerPath + " · stdlib: " + stdlibLabel,
    6000,
  );
}

async function setActiveCompiler(session) {
  const workspaceFolder = session.workspaceFolder();
  const known = discoveredCompilers(workspaceFolder);
  const items = await Promise.all(
    known.map(async (compilerPath) => {
      const context = await queryCompilerContext(compilerPath);
      const stdlib = (context && context.stdlib) || stdlibFromCompiler(compilerPath);
      const version = context && context.version ? "sere " + context.version : "compiler";
      return {
        label: path.basename(path.dirname(compilerPath)) + path.sep + path.basename(compilerPath),
        description: compilerPath,
        detail: stdlib ? version + " · stdlib: " + stdlib : version + " · no stdlib reported",
        compilerPath,
      };
    }),
  );
  items.push({
    label: "Browse…",
    description: "Pick a sere executable",
    detail: "The selected compiler reports its stdlib and toolchain via --print-env",
    browse: true,
  });
  const picked = await vscode.window.showQuickPick(items, {
    title: "Sere: Set Active Compiler",
    placeHolder: "Choose the compiler the language server and commands should use",
    ignoreFocusOut: true,
  });
  if (!picked) {
    return;
  }
  let compilerPath = picked.compilerPath;
  if (picked.browse) {
    const filters = process.platform === "win32" ? { Executable: ["exe"] } : undefined;
    const uris = await vscode.window.showOpenDialog({
      canSelectFiles: true,
      canSelectFolders: false,
      canSelectMany: false,
      filters,
      title: "Select sere",
    });
    if (!uris || uris.length === 0) {
      return;
    }
    compilerPath = uris[0].fsPath;
  }
  if (!compilerPath || !fs.existsSync(compilerPath)) {
    vscode.window.showErrorMessage("Sere: that compiler path does not exist.");
    return;
  }
  await applyActiveCompiler(compilerPath, session);
}

async function pickStdlibPath(session) {
  const uris = await vscode.window.showOpenDialog({
    canSelectFiles: false,
    canSelectFolders: true,
    canSelectMany: false,
    title: "Select Sere stdlib folder (must contain prelude.sere)",
    openLabel: "Use this stdlib",
  });
  if (!uris || uris.length === 0) {
    return;
  }
  const folder = uris[0].fsPath;
  if (!hasPrelude(folder)) {
    vscode.window.showErrorMessage(
      "Sere: that folder is not a stdlib. It must contain prelude.sere.",
    );
    return;
  }
  const config = vscode.workspace.getConfiguration("sere");
  await config.update("stdlibPath", folder, settingsTarget());
  if (session) {
    await session.restart();
  }
  vscode.window.setStatusBarMessage("Sere stdlib: " + folder, 6000);
}

async function openSereSettings(session) {
  const workspaceFolder = session.workspaceFolder();
  const compilerPath = findSere(workspaceFolder, true);
  const stdlib = findStdlib(workspaceFolder, compilerPath);
  const config = vscode.workspace.getConfiguration("sere");
  const picked = await vscode.window.showQuickPick(
    [
      {
        label: "Set Active Compiler",
        description: compilerPath,
        detail: "Pick sere.exe; stdlib is loaded from that compiler unless overridden",
        action: "compiler",
      },
      {
        label: "Set Stdlib Folder",
        description: stdlib || "(not found)",
        detail: config.get("stdlibPath")
          ? "Custom sere.stdlibPath — pick another folder or clear it in Settings"
          : "Pick the folder that contains prelude.sere",
        action: "stdlib",
      },
      {
        label: (config.get("codeLens") ? "Disable" : "Enable") + " Code Lens",
        description: "sere.codeLens",
        action: "codelens",
      },
      {
        label: "Restart Language Server",
        action: "restart",
      },
    ],
    {
      title: "Sere Settings",
      placeHolder: "Sere settings (does not open the VS Code Settings UI)",
      ignoreFocusOut: true,
    },
  );
  if (!picked) {
    return;
  }
  if (picked.action === "compiler") {
    await setActiveCompiler(session);
    return;
  }
  if (picked.action === "stdlib") {
    await pickStdlibPath(session);
    return;
  }
  if (picked.action === "codelens") {
    const enabled = !config.get("codeLens");
    await config.update("codeLens", enabled, settingsTarget());
    vscode.window.setStatusBarMessage("Sere code lens " + (enabled ? "enabled" : "disabled"), 3000);
    return;
  }
  if (picked.action === "restart") {
    session.restart();
  }
}

function compilerEnv(workspaceFolder, compilerPath) {
  const env = { ...process.env };
  const context = contextFor(compilerPath) || activeCompilerContext;
  const stdlib = findStdlib(workspaceFolder, compilerPath);
  if (stdlib.length > 0) {
    env.SERE_STDLIB = stdlib;
  }
  if (context && context.llvmDir) {
    env.SERE_LLVM_DIR = context.llvmDir;
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

function fromLspCompletionKind(kind) {
  if (typeof kind !== "number") {
    return vscode.CompletionItemKind.Text;
  }
  return Math.max(0, kind - 1);
}

function replaceRangeAfterDot(document, position) {
  const line = document.lineAt(position.line).text;
  const before = line.slice(0, position.character);
  const dot = before.lastIndexOf(".");
  const start =
    dot >= 0
      ? new vscode.Position(position.line, dot + 1)
      : document.getWordRangeAtPosition(position)
        ? document.getWordRangeAtPosition(position).start
        : position;
  return new vscode.Range(start, position);
}

function toCompletion(item, document, position) {
  const completion = new vscode.CompletionItem(item.label, fromLspCompletionKind(item.kind));
  completion.detail = item.detail || "";
  completion.insertText = item.insertText || item.label;
  completion.filterText = item.filterText || item.label;
  completion.sortText = item.sortText || item.label;
  if (item.insertTextFormat === 2) {
    completion.insertText = new vscode.SnippetString(String(item.insertText || item.label));
  }
  completion.range = replaceRangeAfterDot(document, position);
  return completion;
}

function documentPosition(document, position) {
  return {
    textDocument: { uri: document.uri.toString() },
    position: toPosition(position),
    sereLine: document.lineAt(position.line).text,
    sereCharacter: position.character,
  };
}

class SereLanguageClient {
  constructor(context) {
    this.context = context;
    this.client = null;
    this.child = null;
    this.stopping = false;
    this.changeTimers = new Map();
    this.diagnostics = vscode.languages.createDiagnosticCollection("sere");
    this.semanticTokensEmitter = new vscode.EventEmitter();
    this.onDidChangeSemanticTokens = this.semanticTokensEmitter.event;
    this.status = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    this.status.command = "sere.restartLanguageServer";
    this.status.text = "Sere";
    this.status.tooltip = "Sere language server — click to restart";
    this.status.show();
    context.subscriptions.push(this.diagnostics, this.status, this.semanticTokensEmitter);
  }

  workspaceFolder() {
    return vscode.workspace.workspaceFolders
      ? vscode.workspace.workspaceFolders[0].uri.fsPath
      : undefined;
  }

  start() {
    this.stopping = false;
    const workspaceFolder = this.workspaceFolder();
    const sere = findSere(workspaceFolder, true);
    this.status.text = "Sere";
    this.status.tooltip = "Sere language server: " + sere;
    const launch = (context) => {
      if (this.stopping) {
        return;
      }
      if (context) {
        activeCompilerContext = context;
      }
      const reported = contextFor(sere) || context;
      this.status.tooltip = reported
        ? "Sere " +
          (reported.version || "") +
          "\n" +
          sere +
          "\nstdlib: " +
          (reported.stdlib || "")
        : "Sere language server: " + sere;
      this.child = spawn(sere, ["--lsp"], {
        stdio: ["pipe", "pipe", "pipe"],
        env: compilerEnv(workspaceFolder, sere),
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
      const filePath = uri.fsPath.replace(/\\/g, "/").toLowerCase();
      if (filePath.endsWith("/prelude.sere") || filePath.includes("/stdlib/")) {
        this.semanticTokensEmitter.fire();
      }
    };
    this.client
      .request("initialize", {
        processId: process.pid,
        rootUri: workspaceFolder ? vscode.Uri.file(workspaceFolder).toString() : null,
        initializationOptions: {
          stdlib: findStdlib(workspaceFolder, sere),
          compiler: (reported && reported.compiler) || sere,
          version: (reported && reported.version) || "",
          llvmDir: (reported && reported.llvmDir) || "",
        },
        capabilities: {
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
        for (const document of vscode.workspace.textDocuments) {
          this.openDocument(document);
        }
      })
      .catch((error) => {
        this.status.text = "Sere $(error)";
        vscode.window.showErrorMessage(`Sere language server initialize failed: ${error.message}`);
      });
    };
    queryCompilerContext(sere).then(launch, () => launch(null));
  }

  async stop() {
    this.stopping = true;
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

  syncDocument(document, skipAnalyze) {
    if (document.languageId !== "sere") {
      return;
    }
    const uri = document.uri.toString();
    this.notify("textDocument/didChange", {
      textDocument: { uri, version: document.version },
      contentChanges: [{ text: document.getText() }],
      skipAnalyze: Boolean(skipAnalyze),
    });
  }

  scheduleAnalyze(document) {
    if (document.languageId !== "sere") {
      return;
    }
    const uri = document.uri.toString();
    const previous = this.changeTimers.get(uri);
    if (previous) {
      clearTimeout(previous);
    }
    const timer = setTimeout(() => {
      this.changeTimers.delete(uri);
      this.syncDocument(document, false);
    }, 250);
    this.changeTimers.set(uri, timer);
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
    vscode.commands.registerCommand("sere.refreshBin", () =>
      runSereCommand(session, ["refresh-bin"], "refreshed ./bin", true),
    ),
    vscode.commands.registerCommand("sere.setActiveCompiler", () => setActiveCompiler(session)),
    vscode.commands.registerCommand("sere.setStdlibPath", () => pickStdlibPath(session)),
    vscode.commands.registerCommand("sere.openSettings", () => openSereSettings(session)),
    vscode.workspace.onDidOpenTextDocument((document) => session.openDocument(document)),
    ...(() => {
      const notifyLibrary = (uri, type) => {
        session.notify("workspace/didChangeWatchedFiles", {
          changes: [{ uri: uri.toString(), type }],
        });
        session.semanticTokensEmitter.fire();
      };
      const watchers = [
        vscode.workspace.createFileSystemWatcher("**/stdlib/**/*.sere"),
        vscode.workspace.createFileSystemWatcher("**/prelude.sere"),
      ];
      for (const watcher of watchers) {
        watcher.onDidCreate((uri) => notifyLibrary(uri, 1));
        watcher.onDidChange((uri) => notifyLibrary(uri, 2));
        watcher.onDidDelete((uri) => notifyLibrary(uri, 3));
      }
      return watchers;
    })(),
    vscode.workspace.onDidChangeTextDocument((event) => {
      if (event.document.languageId !== "sere") {
        return;
      }
      session.syncDocument(event.document, true);
      session.scheduleAnalyze(event.document);
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
          session.syncDocument(document, true);
          return session
            .request("textDocument/completion", documentPosition(document, position))
            .then((result) => {
              const items = Array.isArray(result) ? result : [];
              return new vscode.CompletionList(
                items.map((item) => toCompletion(item, document, position)),
                false,
              );
            });
        },
      },
      ".",
      " ",
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
        onDidChangeSemanticTokens: session.onDidChangeSemanticTokens,
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
