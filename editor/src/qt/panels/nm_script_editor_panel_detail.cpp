#include "nm_script_editor_panel_detail.hpp"
#include <QRegularExpression>

namespace NovelMind::editor::qt::detail {

QStringList buildCompletionWords() {
  return {"and",        "or",     "not",        "true",       "false",       "if",
          "else",       "then",   "scene",      "character",  "choice",      "goto",
          "say",        "set",    "flag",       "show",       "hide",        "with",
          "transition", "wait",   "play",       "stop",       "music",       "sound",
          "voice",      "at",     "background", "left",       "center",      "right",
          "loop",       "fade",   "dissolve",   "slide_left", "slide_right", "slide_up",
          "slide_down", "shake",  "flash",      "fade_to",    "fade_from",   "move",
          "scale",      "rotate", "textbox",    "set_speed",  "allow_skip",  "duration",
          "intensity",  "color",  "to"};
}

QList<SnippetTemplate> buildSnippetTemplates() {
  QList<SnippetTemplate> templates;

  // Scene template
  templates.append({"Scene Block",
                    "scene",
                    "Create a new scene with dialogue",
                    "scene ${1:scene_name} {\n  say ${2:Narrator} \"${3:Description}\"\n}\n",
                    {"scene_name", "Narrator", "Description"}});

  // Character declaration
  templates.append({"Character Declaration",
                    "character",
                    "Declare a new character with properties",
                    "character ${1:CharName}(name=\"${2:Display Name}\", "
                    "color=\"${3:#4A9FD9}\")\n",
                    {"CharName", "Display Name", "#4A9FD9"}});

  // Say dialogue
  templates.append({"Say Dialogue",
                    "say",
                    "Character speaks dialogue",
                    "say ${1:Character} \"${2:Dialogue text}\"\n",
                    {"Character", "Dialogue text"}});

  // Choice block
  templates.append({"Choice Block",
                    "choice",
                    "Present interactive options to player",
                    "choice {\n  \"${1:Option 1}\" -> ${2:scene_target1}\n  \"${3:Option "
                    "2}\" -> ${4:scene_target2}\n}\n",
                    {"Option 1", "scene_target1", "Option 2", "scene_target2"}});

  // If/else block
  templates.append({"If/Else Block",
                    "if",
                    "Conditional branch based on expression",
                    "if ${1:flag condition} {\n  ${2:// true branch}\n} else {\n  "
                    "${3:// false branch}\n}\n",
                    {"flag condition", "// true branch", "// false branch"}});

  // Goto statement
  templates.append({"Goto Statement",
                    "goto",
                    "Jump to another scene",
                    "goto ${1:scene_name}\n",
                    {"scene_name"}});

  // Show background
  templates.append({"Show Background",
                    "showbg",
                    "Display a background image",
                    "show background \"${1:background_id}\"\n",
                    {"background_id"}});

  // Show character
  templates.append({"Show Character",
                    "showchar",
                    "Display a character at position",
                    "show ${1:character} at ${2:center}\n",
                    {"character", "center"}});

  // Hide character
  templates.append({"Hide Character",
                    "hide",
                    "Hide a character or element",
                    "hide ${1:character}\n",
                    {"character"}});

  // Play music
  templates.append({"Play Music",
                    "playmusic",
                    "Start playing background music",
                    "play music \"${1:music_id}\" loop=${2:true}\n",
                    {"music_id", "true"}});

  // Play sound
  templates.append({"Play Sound",
                    "playsound",
                    "Play a sound effect",
                    "play sound \"${1:sound_id}\"\n",
                    {"sound_id"}});

  // Play voice
  templates.append({"Play Voice",
                    "playvoice",
                    "Play voice acting",
                    "play voice \"${1:voice_id}\"\n",
                    {"voice_id"}});

  // Wait
  templates.append({"Wait", "wait", "Pause execution for duration", "wait ${1:1.0}\n", {"1.0"}});

  // Transition
  templates.append({"Transition",
                    "transition",
                    "Apply a visual transition",
                    "transition ${1:fade} ${2:0.5}\n",
                    {"fade", "0.5"}});

  // Set variable
  templates.append({"Set Variable",
                    "setvar",
                    "Assign a value to a variable",
                    "set ${1:variable} = ${2:value}\n",
                    {"variable", "value"}});

  // Set flag
  templates.append({"Set Flag",
                    "setflag",
                    "Set a boolean flag",
                    "set flag ${1:flag_name} = ${2:true}\n",
                    {"flag_name", "true"}});

  // Move character
  templates.append({"Move Character",
                    "move",
                    "Move character to position over time",
                    "move ${1:character} to (${2:0.5}, ${3:0.5}) ${4:1.0}\n",
                    {"character", "0.5", "0.5", "1.0"}});

  return templates;
}

QHash<QString, QString> buildHoverDocs() {
  QHash<QString, QString> docs;
  docs.insert("character", "Declare a character with properties.");
  docs.insert("scene", "Define a scene block with statements.");
  docs.insert("say", "Display dialogue: say <character> \"text\".");
  docs.insert("show", "Show a background or character in the scene.");
  docs.insert("hide", "Hide a background or character.");
  docs.insert("choice", "Present choices: choice { \"Option\" -> action }.");
  docs.insert("goto", "Jump to another scene.");
  docs.insert("set", "Assign a variable or flag: set [flag] name = expr.");
  docs.insert("flag", "Flag access or assignment in conditions/sets.");
  docs.insert("if", "Conditional branch: if expr { ... }.");
  docs.insert("else", "Fallback branch after if.");
  docs.insert("wait", "Pause execution for seconds.");
  docs.insert("play", "Play music/sound/voice.");
  docs.insert("stop", "Stop music/sound/voice.");
  docs.insert("music", "Audio channel for background music.");
  docs.insert("sound", "Audio channel for sound effects.");
  docs.insert("voice", "Audio channel for voice lines.");
  docs.insert("transition", "Transition effect: transition <id> <seconds>.");
  docs.insert("dissolve", "Blend between scenes.");
  docs.insert("fade", "Fade to/from current scene.");
  docs.insert("at", "Position helper for show/move commands.");
  docs.insert("with", "Apply expression/variant to a character.");
  docs.insert("left", "Left position.");
  docs.insert("center", "Center position.");
  docs.insert("right", "Right position.");
  docs.insert("slide_left", "Slide transition to the left.");
  docs.insert("slide_right", "Slide transition to the right.");
  docs.insert("slide_up", "Slide transition up.");
  docs.insert("slide_down", "Slide transition down.");
  docs.insert("shake", "Screen shake effect.");
  docs.insert("flash", "Screen flash effect.");
  docs.insert("fade_to", "Fade to color.");
  docs.insert("fade_from", "Fade from color.");
  docs.insert("move", "Move a character to a position over time.");
  docs.insert("scale", "Scale a character over time.");
  docs.insert("rotate", "Rotate a character over time.");
  docs.insert("background", "Background asset identifier.");
  docs.insert("textbox", "Show or hide the dialogue textbox.");
  docs.insert("set_speed", "Set typewriter speed (chars/sec).");
  docs.insert("allow_skip", "Enable or disable skip mode.");
  return docs;
}

QHash<QString, QString> buildDocHtml() {
  QHash<QString, QString> docs;
  docs.insert("scene", "<h3>scene</h3>"
                       "<p>Define a scene block with statements.</p>"
                       "<p><b>Usage:</b> <code>scene &lt;id&gt; { ... }</code></p>"
                       "<pre>scene main {\n    \"Hello, world!\"\n}</pre>");
  docs.insert("character", "<h3>character</h3>"
                           "<p>Declare a character with display properties.</p>"
                           "<p><b>Usage:</b> <code>character &lt;id&gt;(name=\"Name\")</code></p>"
                           "<pre>character Hero(name=\"Alex\", color=\"#00AAFF\")</pre>");
  docs.insert("say", "<h3>say</h3>"
                     "<p>Display dialogue for a character.</p>"
                     "<p><b>Usage:</b> <code>say &lt;character&gt; \"text\"</code></p>"
                     "<pre>say hero \"We should go.\"</pre>");
  docs.insert("choice", "<h3>choice</h3>"
                        "<p>Present interactive options.</p>"
                        "<p><b>Usage:</b> <code>choice { \"Option\" -> scene_id }</code></p>"
                        "<pre>choice {\n    \"Go left\" -> left_path\n    \"Go right\" -> "
                        "right_path\n}</pre>");
  docs.insert("show", "<h3>show</h3>"
                      "<p>Show a background or character.</p>"
                      "<p><b>Usage:</b> <code>show background \"id\"</code></p>"
                      "<p><b>Usage:</b> <code>show &lt;character&gt; at left</code></p>"
                      "<p><b>Usage:</b> <code>show &lt;character&gt; at (x, y) with "
                      "\"expr\"</code></p>");
  docs.insert("hide", "<h3>hide</h3>"
                      "<p>Hide a background or character.</p>"
                      "<p><b>Usage:</b> <code>hide &lt;id&gt;</code></p>");
  docs.insert("set", "<h3>set</h3>"
                     "<p>Assign a variable or flag.</p>"
                     "<p><b>Usage:</b> <code>set name = expr</code></p>"
                     "<pre>set affection = affection + 5</pre>");
  docs.insert("flag", "<h3>flag</h3>"
                      "<p>Access or set boolean flags.</p>"
                      "<p><b>Usage:</b> <code>set flag has_key = true</code></p>"
                      "<pre>if flag has_key { ... }</pre>");
  docs.insert("if", "<h3>if</h3>"
                    "<p>Conditional branch.</p>"
                    "<p><b>Usage:</b> <code>if expr { ... }</code></p>");
  docs.insert("else", "<h3>else</h3>"
                      "<p>Fallback branch after if.</p>");
  docs.insert("goto", "<h3>goto</h3>"
                      "<p>Jump to another scene.</p>"
                      "<p><b>Usage:</b> <code>goto scene_id</code></p>");
  docs.insert("play", "<h3>play</h3>"
                      "<p>Play music, sound, or voice.</p>"
                      "<p><b>Usage:</b> <code>play music \"file.ogg\"</code></p>"
                      "<p><b>Options:</b> <code>loop=false</code></p>");
  docs.insert("stop", "<h3>stop</h3>"
                      "<p>Stop music, sound, or voice.</p>"
                      "<p><b>Usage:</b> <code>stop music</code></p>"
                      "<p><b>Options:</b> <code>fade=1.0</code></p>");
  docs.insert("transition", "<h3>transition</h3>"
                            "<p>Run a visual transition.</p>"
                            "<p><b>Usage:</b> <code>transition fade 0.5</code></p>"
                            "<p><b>Types:</b> fade, dissolve, slide_left, slide_right, "
                            "slide_up, slide_down</p>");
  docs.insert("slide_left", "<h3>slide_left</h3>"
                            "<p>Slide transition to the left.</p>");
  docs.insert("slide_right", "<h3>slide_right</h3>"
                             "<p>Slide transition to the right.</p>");
  docs.insert("slide_up", "<h3>slide_up</h3>"
                          "<p>Slide transition upward.</p>");
  docs.insert("slide_down", "<h3>slide_down</h3>"
                            "<p>Slide transition downward.</p>");
  docs.insert("dissolve", "<h3>dissolve</h3>"
                          "<p>Blend between scenes.</p>"
                          "<p><b>Usage:</b> <code>dissolve 0.4</code></p>");
  docs.insert("fade", "<h3>fade</h3>"
                      "<p>Fade to/from the current scene.</p>"
                      "<p><b>Usage:</b> <code>fade 0.6</code></p>");
  docs.insert("wait", "<h3>wait</h3>"
                      "<p>Pause execution for seconds.</p>"
                      "<p><b>Usage:</b> <code>wait 0.5</code></p>");
  docs.insert("shake", "<h3>shake</h3>"
                       "<p>Screen shake effect.</p>"
                       "<p><b>Usage:</b> <code>shake 0.4 0.2</code></p>");
  docs.insert("flash", "<h3>flash</h3>"
                       "<p>Flash the screen.</p>"
                       "<p><b>Usage:</b> <code>flash 0.4</code></p>");
  docs.insert("fade_to", "<h3>fade_to</h3>"
                         "<p>Fade to color.</p>"
                         "<p><b>Usage:</b> <code>fade_to #000000 0.3</code></p>");
  docs.insert("fade_from", "<h3>fade_from</h3>"
                           "<p>Fade from color.</p>"
                           "<p><b>Usage:</b> <code>fade_from #000000 0.3</code></p>");
  docs.insert("move", "<h3>move</h3>"
                      "<p>Move a character to a position over time.</p>"
                      "<p><b>Usage:</b> <code>move hero to (0.5, 0.3) 1.0</code></p>");
  docs.insert("scale", "<h3>scale</h3>"
                       "<p>Scale a character over time.</p>"
                       "<p><b>Usage:</b> <code>scale hero 1.2 0.5</code></p>");
  docs.insert("rotate", "<h3>rotate</h3>"
                        "<p>Rotate a character over time.</p>"
                        "<p><b>Usage:</b> <code>rotate hero 15 0.3</code></p>");
  docs.insert("background", "<h3>background</h3>"
                            "<p>Background asset identifier.</p>"
                            "<p><b>Usage:</b> <code>show background \"bg_id\"</code></p>");
  docs.insert("textbox", "<h3>textbox</h3>"
                         "<p>Show or hide the dialogue textbox.</p>"
                         "<p><b>Usage:</b> <code>textbox show</code></p>");
  docs.insert("set_speed", "<h3>set_speed</h3>"
                           "<p>Set typewriter speed (chars/sec).</p>"
                           "<p><b>Usage:</b> <code>set_speed 30</code></p>");
  docs.insert("allow_skip", "<h3>allow_skip</h3>"
                            "<p>Enable or disable skip mode.</p>"
                            "<p><b>Usage:</b> <code>allow_skip true</code></p>");
  return docs;
}

QList<NMScriptEditor::CompletionEntry> buildKeywordEntries() {
  QList<NMScriptEditor::CompletionEntry> entries;
  for (const auto& word : buildCompletionWords()) {
    entries.push_back({word, "keyword"});
  }
  return entries;
}

QList<NMScriptEditor::CompletionEntry>
getContextCompletions(CompletionContext context, const QHash<QString, QString>& scenes,
                      const QHash<QString, QString>& characters,
                      const QHash<QString, QString>& flags,
                      const QHash<QString, QString>& variables, const QStringList& backgrounds,
                      const QStringList& music, const QStringList& voices) {
  QList<NMScriptEditor::CompletionEntry> entries;

  auto addEntry = [&entries](const QString& text, const QString& detail) {
    NMScriptEditor::CompletionEntry entry;
    entry.text = text;
    entry.detail = detail;
    entries.append(entry);
  };

  switch (context) {
  case CompletionContext::AfterSay:
    // Suggest character names
    for (const auto& name : characters.keys()) {
      addEntry(name, "character");
    }
    addEntry("Narrator", "narrator");
    break;

  case CompletionContext::AfterGoto:
  case CompletionContext::AfterScene:
    // Suggest scene names
    for (const auto& name : scenes.keys()) {
      addEntry(name, "scene");
    }
    break;

  case CompletionContext::AfterShow:
    // Suggest backgrounds and characters
    addEntry("background", "keyword");
    for (const auto& name : characters.keys()) {
      addEntry(name, "character");
    }
    for (const auto& bg : backgrounds) {
      addEntry(bg, "background");
    }
    break;

  case CompletionContext::AfterHide:
    // Suggest characters
    for (const auto& name : characters.keys()) {
      addEntry(name, "character");
    }
    break;

  case CompletionContext::AfterPlay:
    // Suggest channel types
    addEntry("music", "channel");
    addEntry("sound", "channel");
    addEntry("voice", "channel");
    break;

  case CompletionContext::AfterStop:
    addEntry("music", "channel");
    addEntry("sound", "channel");
    addEntry("voice", "channel");
    break;

  case CompletionContext::AfterSet:
    // Suggest existing variables and flags
    addEntry("flag", "keyword");
    for (const auto& name : variables.keys()) {
      addEntry(name, "variable");
    }
    for (const auto& name : flags.keys()) {
      addEntry(name, "flag");
    }
    break;

  case CompletionContext::AfterIf:
    // Suggest flags and variables for conditions
    addEntry("flag", "keyword");
    addEntry("not", "keyword");
    for (const auto& name : flags.keys()) {
      addEntry(name, "flag");
    }
    for (const auto& name : variables.keys()) {
      addEntry(name, "variable");
    }
    break;

  case CompletionContext::AfterAt:
    // Suggest positions
    addEntry("left", "position");
    addEntry("center", "position");
    addEntry("right", "position");
    break;

  case CompletionContext::AfterTransition:
    // Suggest transition types
    addEntry("fade", "transition");
    addEntry("dissolve", "transition");
    addEntry("slide_left", "transition");
    addEntry("slide_right", "transition");
    addEntry("slide_up", "transition");
    addEntry("slide_down", "transition");
    break;

  case CompletionContext::AfterChoice:
    // Inside choice - suggest arrow syntax
    addEntry("->", "operator");
    for (const auto& name : scenes.keys()) {
      addEntry(name, "scene");
    }
    break;

  case CompletionContext::InString:
    // Suggest asset paths
    for (const auto& bg : backgrounds) {
      addEntry(bg, "background");
    }
    for (const auto& m : music) {
      addEntry(m, "music");
    }
    for (const auto& v : voices) {
      addEntry(v, "voice");
    }
    break;

  case CompletionContext::InComment:
    // No suggestions in comments
    break;

  case CompletionContext::Unknown:
  default:
    // Return all keywords
    return buildKeywordEntries();
  }

  return entries;
}

QHash<int, QList<QuickFix>> generateQuickFixes(const QList<NMScriptIssue>& issues,
                                               const QString& source) {
  QHash<int, QList<QuickFix>> fixes;

  for (const auto& issue : issues) {
    QList<QuickFix> lineFixes;

    // Check for common error patterns and suggest fixes
    const QString msg = issue.message.toLower();

    // Unknown scene reference
    if (msg.contains("unknown scene") || msg.contains("undefined scene")) {
      QRegularExpression re("scene\\s+([A-Za-z_][A-Za-z0-9_]*)");
      QRegularExpressionMatch match = re.match(msg);
      if (match.hasMatch()) {
        const QString sceneName = match.captured(1);
        lineFixes.append({QString("Create scene '%1'").arg(sceneName), "Add a new scene definition",
                          issue.line, 0,
                          QString("scene %1 {\n  say Narrator \"New "
                                  "scene\"\n}\n\n")
                              .arg(sceneName),
                          0});
      }
    }

    // Unknown character
    if (msg.contains("unknown character") || msg.contains("undefined character")) {
      QRegularExpression re("character\\s+([A-Za-z_][A-Za-z0-9_]*)");
      QRegularExpressionMatch match = re.match(msg);
      if (match.hasMatch()) {
        const QString charName = match.captured(1);
        lineFixes.append({QString("Declare character '%1'").arg(charName),
                          "Add a character declaration at the start", 1, 0,
                          QString("character %1(name=\"%1\", color=\"#4A9FD9\")\n\n").arg(charName),
                          0});
      }
    }

    // Missing closing brace
    if (msg.contains("expected '}'") || msg.contains("missing '}'")) {
      lineFixes.append({"Add missing '}'", "Insert closing brace", issue.line, 0, "}\n", 0});
    }

    // Missing opening brace
    if (msg.contains("expected '{'") || msg.contains("missing '{'")) {
      lineFixes.append({"Add missing '{'", "Insert opening brace", issue.line, 0, " {\n", 0});
    }

    // Missing quotes
    if (msg.contains("expected '\"'") || msg.contains("unterminated string")) {
      lineFixes.append({"Close string", "Add closing quote", issue.line, 0, "\"", 0});
    }

    // Typo suggestions (common misspellings)
    const QHash<QString, QString> typoFixes = {
        {"scnee", "scene"},           {"charater", "character"},   {"choise", "choice"},
        {"backgorund", "background"}, {"trasition", "transition"}, {"disolve", "dissolve"},
        {"centter", "center"},        {"rigth", "right"}};

    for (auto it = typoFixes.constBegin(); it != typoFixes.constEnd(); ++it) {
      if (msg.contains(it.key()) || source.mid(0, 200).contains(it.key(), Qt::CaseInsensitive)) {
        lineFixes.append({QString("Replace '%1' with '%2'").arg(it.key(), it.value()), "Fix typo",
                          issue.line, 0, it.value(), static_cast<int>(it.key().length())});
      }
    }

    if (!lineFixes.isEmpty()) {
      fixes[issue.line] = lineFixes;
    }
  }

  return fixes;
}

QString getSyntaxHintForKeyword(const QString& keyword) {
  static const QHash<QString, QString> hints = {
      {"scene", "scene <name> { <statements> }"},
      {"character", "character <id>(name=\"Name\", color=\"#RRGGBB\")"},
      {"say", "say <character> \"<dialogue>\""},
      {"show", "show background \"id\" | show <char> at <pos>"},
      {"hide", "hide <character>"},
      {"choice", "choice { \"Option\" -> <scene> }"},
      {"goto", "goto <scene_name>"},
      {"if", "if <condition> { ... } else { ... }"},
      {"set", "set <variable> = <value> | set flag <name> = true/false"},
      {"play", "play music|sound|voice \"id\" [loop=true]"},
      {"stop", "stop music|sound|voice [fade=1.0]"},
      {"wait", "wait <seconds>"},
      {"transition", "transition fade|dissolve|slide_* <duration>"},
      {"move", "move <char> to (<x>, <y>) <duration>"},
      {"at", "at left | center | right | (<x>, <y>)"},
      {"with", "with \"expression_name\""},
      {"fade", "fade <duration>"},
      {"dissolve", "dissolve <duration>"},
      {"flag", "flag <name> (in conditions or set statements)"}};

  const QString key = keyword.toLower();
  if (hints.contains(key)) {
    return hints.value(key);
  }
  return QString();
}

} // namespace NovelMind::editor::qt::detail
