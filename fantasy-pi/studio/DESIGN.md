# Fantasy Pi Studio — Architekturspezifikation v1.0

## 1. Gesamtarchitektur

Die IDE ("Fantasy Pi Studio") ist eine WPF-Anwendung (.NET Framework 4.7.2) mit MVVM-Architektur. Sie gliedert sich in folgende Schichten:

| Schicht | Verantwortung | Technologie |
|---------|--------------|-------------|
| UI | Views, Docking, Toolbars, Dialoge | WPF, AvalonDock, AvalonEdit |
| ViewModel | Präsentationslogik, Commands | MVVM (eigene Basis), ReactiveUI-Light |
| Services | Build, Code-Gen, Emulator-Steuerung, KI-Agent | C# Dienste |
| Core/Domain | Projektmodell, Assetmodell, Build-Konfiguration | C# POCOs |
| Integration | Parser, Prozess-Wrapper, File-Watcher | C# mit P/Invoke |

## 2. WPF/MVVM-Projektstruktur

```
FantasyPi.Studio/
├── FantasyPi.Studio.sln
├── src/
│   ├── FantasyPi.Studio/                    # Hauptanwendung (WPF)
│   │   ├── App.xaml, App.xaml.cs
│   │   ├── MainWindow.xaml
│   │   ├── Views/
│   │   │   ├── ProjectExplorerView.xaml
│   │   │   ├── AssetBrowserView.xaml
│   │   │   ├── CodeEditorView.xaml
│   │   │   ├── BuildOutputView.xaml
│   │   │   ├── ErrorListView.xaml
│   │   │   ├── AiAssistantView.xaml
│   │   │   ├── MapEditorView.xaml
│   │   │   ├── SpriteEditorView.xaml
│   │   │   ├── GameObjectEditorView.xaml
│   │   │   ├── AnimationEditorView.xaml
│   │   │   ├── EmulatorHostView.xaml
│   │   │   ├── DebugPanelView.xaml
│   │   │   ├── PropertyInspectorView.xaml
│   │   │   └── StatusBarView.xaml
│   │   ├── ViewModels/
│   │   │   ├── MainWindowViewModel.cs
│   │   │   ├── ProjectExplorerViewModel.cs
│   │   │   ├── AssetBrowserViewModel.cs
│   │   │   ├── CodeEditorViewModel.cs
│   │   │   ├── BuildOutputViewModel.cs
│   │   │   ├── AiAssistantViewModel.cs
│   │   │   └── ...
│   │   └── Converters, Behaviors, Themes
│   ├── FantasyPi.Studio.Core/               # Domain-Modelle, Interfaces
│   │   ├── Models/
│   │   │   ├── ProjectModel.cs
│   │   │   ├── ProjectFile.cs
│   │   │   ├── AssetModel.cs
│   │   │   ├── BuildConfiguration.cs
│   │   │   ├── GameObjectType.cs
│   │   │   ├── MapData.cs
│   │   │   ├── SpriteData.cs
│   │   │   ├── AnimationData.cs
│   │   │   └── FantasyConsoleConfig.cs
│   │   ├── Services/
│   │   │   ├── IProjectService.cs
│   │   │   ├── IBuildService.cs
│   │   │   ├── ICodeGenerationService.cs
│   │   │   ├── IAssetPipelineService.cs
│   │   │   ├── IEmulatorService.cs
│   │   │   ├── IDebugService.cs
│   │   │   └── IAiAgentService.cs
│   │   └── Serialization/
│   │       └── ProjectSerializer.cs
│   ├── FantasyPi.Studio.Services/           # Implementierungen
│   │   ├── ProjectService.cs
│   │   ├── BuildService.cs
│   │   ├── CodeGenerationService.cs
│   │   ├── AssetPipelineService.cs
│   │   ├── EmulatorService.cs
│   │   ├── AiAgentService.cs
│   │   └── ClangCompletionService.cs
│   ├── FantasyPi.Studio.Controls/           # WPF-Custom-Controls
│   │   ├── PixelCanvas.cs
│   │   ├── TileMapGrid.cs
│   │   └── GameObjectCanvas.cs
│   └── FantasyPi.Studio.Tests/              # Unit- und Integrationstests
└── templates/
    ├── EmptyProject/
    ├── Platformer/
    ├── TopDownRPG/
    └── Shooter/
```

## 3. Empfohlene NuGet-Bibliotheken

| Paket | Version | Zweck |
|-------|---------|-------|
| AvalonDock | 3.4.0 | Docking-Manager |
| AvalonEdit | 6.3.0 | Code-Editor mit Syntax-Highlighting |
| Newtonsoft.Json | 13.0.3 | JSON-Serialisierung (.NET 4.7.2 kompatibel) |
| System.Collections.Immutable | 1.5.0 | Immutable Collections |
| Microsoft.Xaml.Behaviors.Wpf | 1.1.39 | Behaviors für MVVM |
| NLog | 5.2.8 | Logging |
| xUnit | 2.4.2 | Tests |
| Moq | 4.18.4 | Mocking |

## 4. Modulübersicht

### 4.1 Projektverwaltung
- **ProjectModel**: Root-Objekt, enthält alle Dateien, Konfigurationen, Assets
- **ProjectFile**: Einzelne Datei mit Typ (Source, Header, Asset, Config)
- **FantasyConsoleConfig**: Hardware-Limits der Zielkonsole (RAM, VRAM, ROM, Sprites, etc.)
- Unterstützt Vorlagen aus `templates/`-Ordner

### 4.2 Sourcecode-Editor
- AvalonEdit als Basis
- Eigener `FantasyCompletionProvider`: nutzt libclang-ähnliches Konzept über einen Hintergrund-Prozess, der Header der GCC-Toolchain und des SDKs parst
- Syntax-Highlighting für C, C++, ASM, .fasm
- Error-Marker aus Build-Output

### 4.3 Build-System
- **BuildService**: Orkestriert GCC Cross-Compiler, Assembler, Linker, Asset-Compiler
- **BuildConfiguration**: Debug, Release, SizeOptimized
- **BuildOutputParser**: Parst GCC-Fehler/Warnungen nach Datei:Zeile:Spalte:Meldung
- Post/Pre-Build-Events als konfigurierbare Shell-Kommandos

### 4.4 Emulator-Integration
- **EmulatorService**: Startet den Host-Emulator als Unterprozess (`fantasy-pi-emulator.exe`)
- **DebugProtocol**: Einfaches JSON-RPC über TCP/localhost zwischen IDE und Emulator
- Views: Register, Memory, VRAM, Disassembly, Call Stack

### 4.5 Asset-Pipeline
- **AssetPipelineService**: Konvertiert PNG→ konsolenformat, WAV→PCM16
- **AssetBrowser**: TreeView mit Drag-and-Drop
- **AutoHeaderGen**: Generiert C++-Header mit Asset-IDs
- Hardware-Limit-Validierung (Farben, Größe, Anzahl)

### 4.6 Editoren
- **SpriteEditor**: PixelCanvas-Control, Palette, Zoom, Onion Skin
- **TilesetEditor**: Tile-Cutter, Collision-Flags, Metadaten
- **MapEditor**: Layer-System, Brush-Tools, Object-Placement, Grid
- **GameObjectEditor**: Typ-Designer, Property Inspector, C++-Stub-Generator
- **AnimationEditor**: Frame-Timeline, State-Machine-Editor

### 4.7 KI-Agent
- **AiAgentService**: Kommuniziert mit OpenAI-kompatibler API
- **ChatSession**: Speichert Kontext pro Projekt
- **CodeDiffEngine**: Zeigt Vorschläge als Patch an
- **ProjectContextBuilder**: Sammelt SDK-Doku + relevante Projektdateien als Kontext

### 4.8 Engine-Konzept (C++-Runtime)
Die Engine ist eine schlanke C++-Bibliothek für die Fantasy-Konsole:
- `GameObject`-Struktur mit Funktionspointer-Callbacks
- `Scene` mit Tilemap-Layern und Object-Listen
- `ResourceManager`: Statische Arrays, keine Heap-Allokation
- `InputManager`, `AudioManager`, `Renderer`: Wrapper um Traps
- Fixed-Point-Math-Utility
- State-Machine-Makros

## 5. Datenmodell (Kernklassen)

```csharp
// ProjectModel.cs
public class ProjectModel
{
    public string Name { get; set; }
    public string FilePath { get; set; }
    public string RootDirectory { get; set; }
    public FantasyConsoleConfig TargetConsole { get; set; }
    public List<ProjectFile> Files { get; set; }
    public List<BuildConfiguration> BuildConfigurations { get; set; }
    public List<AssetFolder> Assets { get; set; }
    public List<GameObjectType> GameObjectTypes { get; set; }
    public List<MapData> Maps { get; set; }
    public AiAssistantConfig AiConfig { get; set; }
    public EmulatorConfig EmulatorConfig { get; set; }
}

// FantasyConsoleConfig.cs
public class FantasyConsoleConfig
{
    public string Name { get; set; } = "Fantasy Pi";
    public uint RamSize { get; set; } = 16 * 1024 * 1024;
    public uint VramSize { get; set; } = 8 * 1024 * 1024;
    public uint RomMaxSize { get; set; } = 32 * 1024 * 1024;
    public uint MaxSprites { get; set; } = 256;
    public uint MaxSpriteWidth { get; set; } = 256;
    public uint MaxSpriteHeight { get; set; } = 256;
    public uint MaxTiles { get; set; } = 1024;
    public uint MaxColorsPerPalette { get; set; } = 256;
    public uint MaxMapWidth { get; set; } = 512;
    public uint MaxMapHeight { get; set; } = 512;
    public uint MaxGameObjectsPerMap { get; set; } = 256;
    public uint MaxAnimations { get; set; } = 256;
    public uint AudioChannels { get; set; } = 8;
    public uint MaxTilesets { get; set; } = 16;
}

// BuildConfiguration.cs
public class BuildConfiguration
{
    public string Name { get; set; }
    public string CompilerPath { get; set; }
    public string AssemblerPath { get; set; }
    public string LinkerPath { get; set; }
    public string CFlags { get; set; }
    public string AsmFlags { get; set; }
    public string LdFlags { get; set; }
    public string LinkerScript { get; set; }
    public List<string> IncludePaths { get; set; }
    public List<string> LibraryPaths { get; set; }
    public List<string> Libraries { get; set; }
    public string PreBuildCommand { get; set; }
    public string PostBuildCommand { get; set; }
    public string OutputRomPath { get; set; }
    public bool OptimizeSize { get; set; }
    public bool DebugSymbols { get; set; }
}

// GameObjectType.cs
public class GameObjectType
{
    public string Id { get; set; }
    public string Name { get; set; }
    public string Description { get; set; }
    public string SpriteAssetId { get; set; }
    public string AnimationId { get; set; }
    public List<GameObjectProperty> Properties { get; set; }
    public List<GameObjectEvent> Events { get; set; }
    public string GeneratedCppFile { get; set; }
    public string GeneratedHeaderFile { get; set; }
}

public class GameObjectProperty
{
    public string Name { get; set; }
    public PropertyType Type { get; set; }
    public string DefaultValue { get; set; }
    public bool ExposedToEditor { get; set; }
}

public enum PropertyType
{
    Int, Float, Bool, String, Enum, Flags,
    AssetRef, SpriteRef, AnimationRef, MapRef,
    ObjectRef, Color, Vector2, Rect
}

public class GameObjectEvent
{
    public string Name { get; set; }
    public string EventType { get; set; } // OnInit, OnUpdate, OnCollision, ...
    public string HandlerFunctionName { get; set; }
}
```

## 6. UI-Konzept

### Layout (MainWindow)
- **Top**: Menüleiste + Toolbar (Build, Run, Debug, AI-Chat)
- **Left**: Project Explorer (oben) + Asset Browser (unten) — dockbar
- **Center**: Document Area (AvalonDock) — Tabs für Editoren
- **Right**: Property Inspector + AI Assistant Panel — dockbar
- **Bottom**: Build Output / Error List / Debug Panel — dockbar
- **StatusBar**: Build-Status, Zeile:Spalte, FPS, Memory-Budget

### Theming
- Dunkles Theme als Standard (VS-like)
- Farben: `#2D2D30` Hintergrund, `#007ACC` Akzent

## 7. Code-Editor-Konzept mit Autovervollständigung

### Architektur
```
CodeEditorView (AvalonEdit)
    └── FantasyCompletionProvider
            ├── ClangCompletionService (Hintergrundprozess)
            │       ├── Parst GCC-Header (stdint.h, stdlib.h, ...)
            │       ├── Parst SDK-Header (fantasy.h, graphics.h, audio.h)
            │       ├── Parst projektinterne Header
            │       └── Bietet Completion-API über NamedPipe/Datei
            └── FallbackCompletionProvider
                    ├── Statische API-Liste (XML/JSON)
                    └── Keyword-Liste
```

### Umsetzung
- Ein C#-Service startet einen schlanken Hintergrund-Prozess (z.B. eine eigene kleine Clang-Bindung oder ein selbstgeschriebener C-Header-Parser)
- Alternative: Da libclang auf .NET 4.7.2 schwierig zu binden ist, implementieren wir einen **eigenen C/C++-Header-Parser** in C# mit ANTLR oder handgeschriebenem Recursive-Descent-Parser
- Der Parser extrahiert: Typedefs, Structs, Enums, Funktionen, Variablen, Macros
- Ergebnisse werden in einem **SymbolCache** (Dictionary<Dateipfad, List<Symbol>>) gehalten
- Dateiänderungen invalidieren den Cache per FileSystemWatcher
- Autocomplete-Trigger: `.`, `->`, `::`, Strg+Leer

### Fallback
- JSON-Datei `sdk_api.json` mit allen Fantasy-Console-Funktionen, Parametern, Beschreibungen
- Wird mit dem SDK mitgeliefert und ist immer verfügbar

## 8. KI-Agent-Architektur

```csharp
public class AiAgentService : IAiAgentService
{
    private readonly HttpClient _httpClient;
    private readonly string _apiBaseUrl;
    private readonly string _apiKey;
    private readonly string _model;
    private readonly List<ChatMessage> _history;

    public async Task<string> SendMessageAsync(string userMessage,
        ProjectContext context,
        bool streamResponse = false,
        CancellationToken ct = default)
    {
        var systemPrompt = BuildSystemPrompt(context);
        var request = new OpenAiChatRequest
        {
            Model = _model,
            Messages = new[] { new ChatMessage("system", systemPrompt) }
                .Concat(_history)
                .Concat(new[] { new ChatMessage("user", userMessage) })
                .ToList(),
            Stream = streamResponse,
            Temperature = 0.3,
            MaxTokens = 4096
        };
        // Sendet an /v1/chat/completions
        // Parst Chunked-Response bei Streaming
    }

    private string BuildSystemPrompt(ProjectContext ctx)
    {
        var sb = new StringBuilder();
        sb.AppendLine("Du bist ein erfahrener C++-Entwickler für die Fantasy Pi Konsole.");
        sb.AppendLine("Die Konsole hat 16MB RAM, 8MB VRAM, 32MB ROM.");
        sb.AppendLine("Nutze die Fantasy-Console-API. Vermeide dynamische Allokation.");
        if (ctx.SdkSummary != null)
            sb.AppendLine(ctx.SdkSummary);
        if (ctx.SelectedCode != null)
            sb.AppendLine($"Ausgewählter Code:\n{ctx.SelectedCode}");
        return sb.ToString();
    }
}
```

### Sicherheitsgrenzen
- Der Agent liest nur Dateien, die der Nutzer explizit ausgewählt oder im Kontext freigegeben hat
- Schreibzugriff nur über "Apply Patch"-Funktion, die Diff anzeigt
- Kein automatisches Speichern ohne Bestätigung

## 9. Map-/Sprite-/Tileset-/Animation-/GameObject-Editor

### MapEditor
- WPF-Canvas mit Grid-Zeichnung
- Layer-Typen: Tile, Collision, Object, Trigger, Parallax
- Tools: Brush (1x1..NxN), Eraser, Fill, Rect, Select
- Tile-Palette aus registrierten Tilesets
- Object-Placement via Drag-and-Drop aus GameObject-Browser
- Export: JSON → C++-Array via CodeGenerationService

### SpriteEditor
- WritableBitmap als Pixel-Buffer
- Palette als Color[]-Array
- Tools: Pencil, Line, Rect, Fill, Pick, Select
- Onion-Skin: Vorheriger/Nächster Frame als halbtransparentes Overlay
- Zoom: 1x..32x via ScaleTransform
- Export: PNG + Metadaten-JSON → Asset-Pipeline → C-Array

### GameObjectEditor
- TreeView der definierten Typen
- PropertyGrid für Properties
- Event-List mit Handler-Zuweisung
- "Generate Code"-Button erzeugt Header + Stub-Implementierung
- Preview: Zeigt Sprite im Editor mit Collision-Box-Overlay

## 10. Engine-Konzept (C++-Runtime)

```cpp
// fantasy_engine.h
#pragma once
#include <stdint.h>
#include "fantasy_vm.h"

namespace fe {

struct Vec2 { int32_t x, y; };
struct Rect { int32_t x, y, w, h; };

struct GameObject {
    uint16_t type_id;
    uint16_t instance_id;
    Vec2 pos;
    Vec2 vel;
    Rect collider;
    uint32_t sprite_id;
    uint32_t anim_id;
    uint32_t state;
    uint32_t flags;
    void* user_data; // Pointer zu typ-spezifischem Struct
};

struct Scene {
    uint32_t map_id;
    uint16_t go_count;
    GameObject objects[256]; // Max 256 pro Scene
    // ... tilemap layers
};

typedef void (*GO_InitFunc)(GameObject* self);
typedef void (*GO_UpdateFunc)(GameObject* self);
typedef void (*GO_DrawFunc)(GameObject* self);
typedef void (*GO_CollisionFunc)(GameObject* self, GameObject* other);
typedef void (*GO_InteractFunc)(GameObject* self, GameObject* other);

struct GameObjectTypeDef {
    uint16_t type_id;
    uint16_t sprite_id;
    uint16_t anim_id;
    uint16_t default_state;
    GO_InitFunc init;
    GO_UpdateFunc update;
    GO_DrawFunc draw;
    GO_CollisionFunc on_collision;
    GO_InteractFunc on_interact;
};

void Engine_Init();
void Engine_LoadScene(uint32_t map_id);
void Engine_Update();   // Ruft alle GO_Update auf
void Engine_Draw();     // Ruft alle GO_Draw auf, dann Tilemaps
void Engine_PollInput();
void Engine_PlaySFX(uint32_t sample_id, uint8_t channel);

// Fixed-point math: 16.16
inline int32_t fp_mul(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 16); }
inline int32_t fp_div(int32_t a, int32_t b) { return (int32_t)(((int64_t)a << 16) / b); }

} // namespace fe
```

## 11. C++ Runtime-/Engine-API-Vorschlag

Die Engine bietet ein datenorientiertes, statisches System:
- **Kein `new`/`delete`** im Spielcode. Alle Objekte sind Arrays.
- **GameObject-Typen** werden per Makro registriert:
```cpp
REGISTER_GAMEOBJECT(EnemySlime, GOID_ENEMY_SLIME)
    .sprite(ASSET_SPR_ENEMY_SLIME)
    .size(16, 16)
    .collider(0, 0, 16, 16)
    .on_init(EnemySlime_Init)
    .on_update(EnemySlime_Update)
    .on_collision(EnemySlime_OnCollision);
```
- **Szenen-Wechsel**: `Engine_LoadScene(MAP_FOREST_01);`
- **Asset-Zugriff**: Alles über generierte IDs (`ASSET_SPR_PLAYER`, etc.)

## 12. Codegenerierungs-Konzept

### Generierte Dateien (ins `generated/`-Verzeichnis)

```
game_project/
├── src/
│   ├── main.cpp              # User-Code
│   ├── player.cpp            # User-Code
│   └── ...
├── generated/
│   ├── asset_ids.h           # #define ASSET_SPR_XXX
│   ├── asset_data.cpp        # Statische Asset-Daten
│   ├── gameobject_types.h    # Enum GOID_XXX
│   ├── gameobject_registry.cpp # Array aller GameObjectTypeDef
│   ├── map_data.cpp          # Statische Tilemap-Arrays
│   ├── collision_data.cpp    # Kollisions-Layer
│   ├── animation_data.cpp    # Frame-Tabellen
│   └── engine_config.h       # #defines für Limits
└── assets/
    ├── sprites/
    ├── tilesets/
    ├── maps/
    └── sounds/
```

### Trennung User-Code / Generated-Code
- **User-Code** darf `generated/*.h` inkludieren, niemals `generated/*.cpp`
- **Generated-Code** darf User-Code-Header nicht inkludieren (außer deklarierte Callbacks)
- **Makro-Technik** für User-Code in generierten Dateien:
```cpp
// gameobject_registry.cpp (generated)
#include "gameobject_user_impl.h" // User-Code-Datei mit Makros

DEFINE_GO_REGISTRY_BEGIN
DEFINE_GO(GOID_PLAYER, ASSET_SPR_PLAYER, Player_Init, Player_Update, Player_Draw)
DEFINE_GO(GOID_ENEMY_SLIME, ASSET_SPR_ENEMY_SLIME, Slime_Init, Slime_Update, nullptr)
DEFINE_GO_REGISTRY_END
```
Der Nutzer implementiert die Funktionen, die Registry wird von der IDE generiert.

## 13. Build-System-Integration mit GCC Cross-Compiler

```csharp
public class BuildService : IBuildService
{
    public async Task<BuildResult> BuildAsync(ProjectModel project,
        BuildConfiguration config,
        IProgress<string> outputProgress,
        CancellationToken ct)
    {
        // 1. Pre-Build: Asset-Pipeline
        await _assetPipeline.ProcessAllAsync(project, ct);
        // 2. Code-Generierung
        await _codeGen.GenerateAsync(project, ct);
        // 3. Compile Sources
        var sourceFiles = project.Files.Where(f => f.IsSource);
        var objectFiles = new List<string>();
        foreach (var src in sourceFiles)
        {
            var obj = Path.ChangeExtension(src.RelativePath, ".o");
            var args = $"{config.CFlags} -c {src.AbsolutePath} -o {obj}";
            var result = await RunProcessAsync(config.CompilerPath, args, outputProgress, ct);
            if (result.ExitCode != 0) return BuildResult.FromFailure(result.Stderr);
            objectFiles.Add(obj);
        }
        // 4. Link
        var linkArgs = $"{config.LdFlags} -T{config.LinkerScript} {string.Join(" ", objectFiles)} -o {config.OutputRomPath}";
        var linkResult = await RunProcessAsync(config.LinkerPath, linkArgs, outputProgress, ct);
        // 5. Post-Build
        if (!string.IsNullOrEmpty(config.PostBuildCommand))
            await RunProcessAsync("cmd.exe", $"/c {config.PostBuildCommand}", outputProgress, ct);
        // 6. Parse Errors
        var errors = ParseGccOutput(result.Stderr);
        return new BuildResult { Success = linkResult.ExitCode == 0, Errors = errors };
    }
}
```

## 14. Emulator-/Debugger-Integration

### Debug-Protokoll (JSON-RPC über TCP)
```json
// IDE -> Emulator
{"method":"setBreakpoint","params":{"address":0x1000}}
{"method":"run","params":{}}
{"method":"step","params":{}}
{"method":"readMemory","params":{"address":0x10000,"size":256}}

// Emulator -> IDE
{"method":"stopped","params":{"reason":"breakpoint","address":0x1000}}
{"method":"memoryData","params":{"address":0x10000,"data":[...]}}
```

### Views
- **Register**: Grid mit R0-R15, SP, LR, PC, Flags
- **Memory**: Hex-Viewer mit ASCII-Spalte, Adress-Sprung
- **VRAM**: Zeigt Tile-/Sprite-Speicher als Bilder
- **Disassembly**: Liste der dekodierten Instruktionen um PC
- **Call Stack**: Liste der LR-Werte

## 15. Asset-Pipeline

### Fluss
```
User droppt PNG in IDE
    -> AssetPipelineService erkennt Typ (Sprite/Tileset)
    -> Vorschau wird generiert
    -> Validierung (Farben <= 256, Größe <= 256x256)
    -> Konvertierung: PNG -> RGBA8888 -> Premultiplied Alpha -> Optional Indexed8
    -> Speicherung als .asset (Binär + Metadaten-JSON)
    -> Code-Gen: Eintrag in asset_ids.h, Daten in asset_data.cpp
```

### Asset-Metadaten-JSON
```json
{
  "id": "spr_player",
  "type": "sprite",
  "source": "sprites/player.png",
  "format": "rgba8888",
  "width": 32,
  "height": 32,
  "premultiplied": true,
  "palette_id": null,
  "generated_header": "ASSET_SPR_PLAYER"
}
```

## 16. Dateiformate

### Projektdatei (.fproj)
```json
{
  "version": 1,
  "name": "Mein Spiel",
  "target_console": "fantasy-pi",
  "console_config": { "ram_size": 16777216, ... },
  "active_build_config": "Debug",
  "build_configs": [...],
  "files": [
    { "path": "src/main.cpp", "type": "source" },
    { "path": "src/player.cpp", "type": "source" },
    { "path": "assets/sprites/player.png", "type": "asset" }
  ],
  "assets": [...],
  "game_objects": [...],
  "maps": [...]
}
```

### Map-Datei (.fmap)
```json
{
  "id": "map_forest_01",
  "name": "Wald Level 1",
  "width": 64,
  "height": 48,
  "tile_width": 16,
  "tile_height": 16,
  "layers": [
    {
      "name": "bg",
      "type": "tile",
      "tileset_id": "ts_ground",
      "data": [0,0,1,1,2,...]
    },
    {
      "name": "collision",
      "type": "collision",
      "data": [0,0,1,1,0,...]
    },
    {
      "name": "objects",
      "type": "object",
      "objects": [
        { "type_id": "player", "x": 128, "y": 256, "properties": {...} }
      ]
    }
  ]
}
```

## 17. Ordnerstruktur des IDE-Projekts
```
FantasyPi.Studio/
├── src/
│   ├── FantasyPi.Studio/
│   ├── FantasyPi.Studio.Core/
│   ├── FantasyPi.Studio.Services/
│   ├── FantasyPi.Studio.Controls/
│   └── FantasyPi.Studio.Tests/
├── lib/                          # Externe DLLs (AvalonDock, etc.)
├── templates/
│   ├── EmptyProject/
│   │   ├── src/main.cpp
│   │   ├── src/game.h
│   │   ├── assets/
│   │   └── project.fproj
│   └── Platformer/
│       ├── src/
│       ├── assets/
│       └── project.fproj
├── tools/
│   └── header_parser/            # Optional: C-Header-Parser-Tool
└── docs/
```

## 18. Ordnerstruktur eines erzeugten Spieleprojekts
```
MeinSpiel/
├── MeinSpiel.fproj
├── src/
│   ├── main.cpp
│   ├── game.cpp
│   ├── game.h
│   └── user_objects/
│       ├── player.cpp
│       └── enemy_slime.cpp
├── generated/
│   ├── asset_ids.h
│   ├── asset_data.cpp
│   ├── gameobject_types.h
│   ├── gameobject_registry.cpp
│   ├── map_data.cpp
│   ├── collision_data.cpp
│   └── animation_data.cpp
├── assets/
│   ├── sprites/
│   ├── tilesets/
│   ├── maps/
│   ├── sounds/
│   └── music/
├── build/
│   └── (Objektdateien, ROM)
└── config/
    └── build_config.json
```

## 19. Wichtige Klassen, Interfaces und Services

```csharp
// IProjectService
public interface IProjectService
{
    ProjectModel CurrentProject { get; }
    event EventHandler ProjectChanged;
    void CreateNew(string templateName, string path);
    void Load(string fprojPath);
    void Save();
    void AddFile(string relativePath, string content = null);
    void RemoveFile(string relativePath);
}

// IBuildService
public interface IBuildService
{
    bool IsBuilding { get; }
    event EventHandler<BuildMessage> BuildMessageReceived;
    Task<BuildResult> BuildAsync(BuildConfiguration config);
    Task CleanAsync();
}

// ICodeGenerationService
public interface ICodeGenerationService
{
    Task GenerateAsync(ProjectModel project);
    string GenerateAssetIds(ProjectModel project);
    string GenerateGameObjectRegistry(ProjectModel project);
    string GenerateMapData(MapData map);
}

// IEmulatorService
public interface IEmulatorService
{
    bool IsRunning { get; }
    void Start(string romPath);
    void Stop();
    void Pause();
    void Step();
    Task<byte[]> ReadMemoryAsync(uint address, uint size);
    event EventHandler<EmulatorState> StateChanged;
}

// IAiAgentService
public interface IAiAgentService
{
    bool IsConfigured { get; }
    Task<string> ChatAsync(string message, ProjectContext context, bool stream = false);
    Task<string> ExplainCodeAsync(string code, ProjectContext context);
    Task<string> GenerateGameObjectAsync(GameObjectType type, ProjectContext context);
    Task<string> AnalyzeBuildErrorsAsync(string buildLog, ProjectContext context);
}
```

## 20. Beispielcode C# (zentrale Komponenten)

Siehe Projektdateien in `FantasyPi.Studio/`.

## 21. Beispielcode C++ (GameObject-Callbacks)

```cpp
// player.cpp (User-Code)
#include "fantasy_engine.h"
#include "generated/asset_ids.h"
#include "generated/gameobject_types.h"

struct PlayerData {
    int32_t speed;
    int32_t jump_vel;
    bool on_ground;
};

void Player_Init(GameObject* self) {
    PlayerData* data = (PlayerData*)self->user_data;
    data->speed = fe::fp_from_int(3);
    data->jump_vel = fe::fp_from_int(-8);
    data->on_ground = false;
    self->sprite_id = ASSET_SPR_PLAYER;
}

void Player_Update(GameObject* self) {
    PlayerData* data = (PlayerData*)self->user_data;
    fe::Engine_PollInput();

    if (fe::Input_IsDown(BTN_RIGHT))
        self->vel.x = data->speed;
    else if (fe::Input_IsDown(BTN_LEFT))
        self->vel.x = -data->speed;
    else
        self->vel.x = 0;

    self->pos.x += self->vel.x;
    self->pos.y += self->vel.y;
}

void Player_OnCollision(GameObject* self, GameObject* other) {
    if (other->type_id == GOID_ENEMY_SLIME) {
        fe::Engine_PlaySFX(ASSET_SFX_HIT, 0);
    }
}
```

## 22. Risiken und technische Herausforderungen

| Risiko | Impact | Mitigation |
|--------|--------|------------|
| C++-Autocomplete ohne libclang | Medium | Eigener Parser für Header + Fallback-JSON |
| WPF-Docking-Komplexität | Low | AvalonDock etabliert |
| Große ROMs bei vielen Assets | Medium | Asset-Kompression, Streaming-Konzept |
| Emulator-Debug-Protokoll | Medium | Einfaches JSON-RPC, iterativ ausbauen |
| .NET 4.7.2 Einschränkungen | Low | Newtonsoft.Json statt System.Text.Json |
| KI-Agent Token-Limits | Medium | Kontext auf relevante Dateien beschränken |
| C++-Code-Gen und User-Code-Synchronisation | High | Klare Trennung, Makro-basierte Registrierung |

## 23. Konkrete nächste Entwicklungsschritte

1. **Grundgerüst**: Solution, Projekte, MVVM-Basis, AvalonDock-MainWindow
2. **Projektsystem**: ProjectModel, Serializer, New/Open/Save, Project Explorer
3. **Code-Editor**: AvalonEdit-Integration, Syntax-Highlighting für C/C++/ASM
4. **Build-System**: BuildService, GCC-Wrapper, Error-Parser, Output-Panel
5. **Asset-Pipeline**: Import, Vorschau, Validierung, ID-Generierung
6. **KI-Agent**: Chat-Panel, OpenAI-API-Integration, Kontext-Builder
7. **Editoren**: Sprite-Editor (PixelCanvas), Map-Editor (TileMapGrid)
8. **Code-Generierung**: Asset-IDs, Map-Daten, GameObject-Registry
9. **Emulator-Integration**: ROM-Start, Grundlegendes Debug-Protokoll
10. **GameObject-System**: Editor, Property-Grid, C++-Stub-Generator

## 24. Sinnvolle Ergänzungen

- **Version Control Integration**: Git-Status in Project Explorer, Diff-View
- **Package Manager**: SDK-Erweiterungen als NuGet-ähnliche Pakete
- **Profiler-Integration**: Zeigt in der IDE an, welche Funktionen viele Zyklen verbrauchen
- **Shader/Effect-Editor**: Falls die Konsole später erweitert wird
- **Localization-System**: Mehrsprachige Texte im Spiel
- **Savegame-Editor**: Visual Editor für persistente Datenstrukturen
- **Achievements/Trophies-System**: Meta-Daten für das Spiel
- **Multiplayer/Netplay**: Falls die Konsole Netzwerk unterstützt
- **Crash-Reporter**: Automatische Fehlerberichte aus dem Emulator
- **Tutorials/Onboarding**: Integrierte Beispielprojekte mit Schritt-für-Schritt-Anleitungen

---

*Diese Spezifikation ist als lebendes Dokument gedacht und wird mit der Implementierung weiterentwickelt.*
