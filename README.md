```
AI_Smart_Surveillance_System/
│
├── CMakeLists.txt
├── README.md
├── assets/                     ← необов’язкове: raw зображення, моделі
│   ├── models/                 ← ONNX-файли (FaceNet, YOLO)
│   ├── test_videos/
│   └── fonts/
│
├── src/
│   ├── main.cpp
│   │
│   ├── core/                   ← основна логіка / ядро
│   │   ├── CameraManager.h/.cpp        ← робота з декількома камерами
│   │   ├── AIProcessor.h/.cpp          ← обробка кадрів, FaceNet, DNN
│   │   ├── HeatmapAccumulator.h/.cpp   ← накопичення "тепла"
│   │   ├── EventLogger.h/.cpp          ← логи AI / система / події
│   │   ├── SupabaseClient.h/.cpp       ← REST API до сервера Drogon/Supabase
│   │   ├── ThreadPool.h/.cpp           ← пул потоків для камер / AI
│   │   └── SettingsManager.h/.cpp      ← конфігурації (QSettings / JSON)
│   │
│   ├── ui/                     ← інтерфейс (вікна, сторінки, панелі)
│   │   ├── MainWindow.h/.cpp          ← головне вікно (QMainWindow)
│   │   ├── LoginWindow.h/.cpp         ← екран входу
│   │   ├── pages/
│   │   │   ├── DashboardPage.h/.cpp
│   │   │   ├── CamerasPage.h/.cpp
│   │   │   ├── HeatmapPage.h/.cpp
│   │   │   ├── EventsPage.h/.cpp
│   │   │   ├── AnalyticsPage.h/.cpp
│   │   │   ├── Face3DPage.h/.cpp
│   │   │   └── SettingsPage.h/.cpp
│   │   ├── widgets/
│   │   │   ├── CameraTile.h/.cpp       ← окремий віджет для однієї камери
│   │   │   ├── ConsoleWidget.h/.cpp    ← консоль знизу
│   │   │   ├── SidebarWidget.h/.cpp    ← ліва панель (режими)
│   │   │   ├── StatusBarWidget.h/.cpp  ← нижня смужка статусу
│   │   │   └── PopupNotification.h/.cpp← спливаючі повідомлення
│   │   └── dialogs/
│   │       ├── CameraConfigDialog.h/.cpp
│   │       └── AboutDialog.h/.cpp
│   │
│   ├── qss/                    ← стилі оформлення (Qt Style Sheets)
│   │   ├── dark.qss
│   │   ├── light.qss
│   │   └── variables.qss       ← кольорові константи
│   │
│   ├── icons/                  ← SVG або PNG іконки для sidebar, toolbar
│   │   ├── dashboard.svg
│   │   ├── camera.svg
│   │   ├── heatmap.svg
│   │   ├── logs.svg
│   │   ├── analytics.svg
│   │   ├── face3d.svg
│   │   ├── console.svg
│   │   └── settings.svg
│   │
│   ├── resources.qrc           ← реєстрація ресурсів (QSS, іконки, шрифти)
│   │
│   └── utils/                  ← дрібні утиліти (формати, конвертації)
│       ├── ImageUtils.h/.cpp
│       ├── JsonUtils.h/.cpp
│       └── TimeUtils.h/.cpp
│
├── tests/                      ← GTest / QtTest
│   ├── test_ai.cpp
│   ├── test_heatmap.cpp
│   ├── test_camera.cpp
│   └── CMakeLists.txt
│
└── config/
    ├── appsettings.json        ← сервер, шляхи, FPS, логування
    ├── supabase.json           ← ключі API (не комітити)
    └── cameras.json            ← список камер (url, id, name)

```


    Опис головних директорій
| Каталог     | Призначення                                                 |
| ----------- | ----------------------------------------------------------- |
| **core/**   | Уся бізнес-логіка: AI, камери, мережа, база, налаштування   |
| **ui/**     | Класи інтерфейсу (головне вікно, сторінки, панелі, діалоги) |
| **qss/**    | Теми оформлення (dark/light)                                |
| **icons/**  | Іконки SVG/PNG, підключені через `.qrc`                     |
| **utils/**  | Утилітарні функції (конвертація JSON, часу, зображень)      |
| **tests/**  | Модульні тести (GoogleTest або QtTest)                      |
| **config/** | Конфігураційні JSON-файли для серверів, камер тощо          |
| **assets/** | AI-моделі (ONNX), тестові відео, шрифти, інше               |



| Модуль            | Виконує                                                   |
| ----------------- | --------------------------------------------------------- |
| `MainWindow`      | збирає `SidebarWidget`, `ConsoleWidget`, `QStackedWidget` |
| `SidebarWidget`   | список режимів (Dashboard, Cameras, Heatmap, ...)         |
| `CamerasPage`     | показує 2×2 / 3×3 потоки                                  |
| `HeatmapPage`     | накладає Heatmap поверх відео                             |
| `AIProcessor`     | обробляє кадри → embeddings / faces                       |
| `SupabaseClient`  | відправляє дані на сервер (Drogon / Supabase)             |
| `EventLogger`     | пише логи у консоль і файл                                |
| `ConsoleWidget`   | відображає лог на екрані                                  |
| `SettingsManager` | читає config JSON і керує QSettings                       |


Кожен модуль = окремий .h/.cpp

Не тримай AI-код усередині UI-класів.

core = вся “мозкова” логіка без QWidget.

ui = тільки візуальна частина.

QSS — підключається в main.cpp або в MainWindow.

Іконки SVG — кладеш у icons/ і підключаєш через ":/icons/name.svg".

Все, що не відображається — поза ui.
## OpenVINO Execution Provider

- The build fetches the Intel.ML.OnnxRuntime.OpenVino 1.23.0 NuGet package and copies all required onnxruntime/openvino DLLs next to the executable.
- OpenVINO GPU inference is enabled on Windows by default. Override the target with the AIP_OPENVINO_DEVICE env variable (e.g., GPU_FP16, GPU_FP32, CPU_FP32).
- Set AIP_DISABLE_OPENVINO=1 to force the CPU path if the Intel GPU drivers or OpenVINO runtime are unavailable.

## Remote server sync

The desktop client now authenticates against `https://myserver-tc2d.onrender.com` via `/auth/login`, fetches `/api/persons`, and pushes detection summaries to `/api/events` roughly every 5 seconds.

At startup the app shows a dedicated login dialog. The credentials entered there are used both for the immediate `/auth/login` call (to obtain a bearer token) and for the background `ServerSyncManager`, so you no longer have to bake secrets into the repository.

Configure credentials and the polling cadence in `config/server.json`:

```json
{
  "base_url": "https://myserver-tc2d.onrender.com",
  "email": "admin@example.com",
  "password": "change-me",
  "sync_interval_ms": 5000
}
```

To keep secrets out of source control, set environment variables instead of touching the JSON:

| Env variable      | Purpose                                |
| ----------------- | -------------------------------------- |
| `SURV_SERVER_URL` | Overrides the API base URL             |
| `SURV_EMAIL`      | Login email for `/auth/login`          |
| `SURV_PASSWORD`   | Login password                         |
| `SURV_SYNC_MS`    | Poll/sync interval (milliseconds)      |

The face database page shows the remote persons table alongside the local embeddings, and the "Reload" button triggers an immediate `/api/persons` fetch. Detection events emitted by `AIProcessor` are queued and POSTed upstream with automatic retries so the backend continues receiving updates, even if frames arrive faster than the network allows. Unknown faces now trigger an on-screen dialog so the operator can either mark them as "unknown" (raising a `/api/alerts` entry) or immediately register the person with name/role via `/api/persons`.

## Diagnostics & logging

- The app now installs a global Qt message handler that writes every `qInfo/qWarning/qCritical` entry to both the console and timestamped files under `logs/` (next to the executable).  
- Each subsystem (`LoginWindow`, `SupabaseClient`, `ServerSyncManager`, camera manager, face DB, etc.) emits structured log lines like `[ServerSync] Event delivered: ...` so you can trace failures even on headless deployments.  
- To inspect the most recent run, open the newest `logs/app_*.log`.


