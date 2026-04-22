# USB FAT32 Creator v1.3.0

Desktopowa aplikacja C++/Qt do bezpiecznego planowania i przygotowania podziału fizycznych nośników USB.

Główna idea:
- tworzenie partycji FAT32 (maks. 32 GB na partycję),
- wykorzystanie pozostałej przestrzeni jako exFAT,
- opcjonalny podział na wiele "wirtualnych penów" FAT32.

## Bezpieczeństwo

To narzędzie operuje na fizycznych dyskach. Traktuj je jak narzędzie administracyjne.

- Aplikacja wykonuje operacje dopiero po wyraźnym potwierdzeniu użytkownika.
- Operacje partycjonowania i formatowania usuwają dane.
- Przed wykonaniem pojawia się wyraźne ostrzeżenie o nieodwracalności zmian.
- Zalecany workflow: `Testuj` -> `Zbuduj plan` -> `Wykonaj`.
- Na Windows aplikacja automatycznie żąda uprawnień administratora (wymagane przez `diskpart`).

## Najważniejsze funkcje

- Wykrywanie fizycznych urządzeń (bez partycji logicznych).
- Budowanie planu podziału FAT32 + exFAT.
- Symulacja planu (`Testuj`) bez zmian na dysku.
- Walidacja planu przed wykonaniem (m.in. limity FAT32, spójność rozmiarów, reguły exFAT).
- Na Windows: automatyczne wykonanie przez `diskpart` — generuje tymczasowy skrypt i uruchamia go.
- Na macOS/Linux: wykonanie przez `diskutil` / `parted` + `mkfs`.

## Wymagania

- CMake 3.21+
- Kompilator C++17
- Qt6 Widgets

## Szybki start

### macOS / Linux

```bash
cmake -S . -B build
cmake --build build -j
./build/usb_fat32_creator
```

### Windows (PowerShell z MinGW + Qt6)

```powershell
# Wymagania: CMake, MinGW-w64, Qt6 (np. przez aqtinstall)
# pip install aqtinstall
# python -m aqt install-qt windows desktop 6.8.3 win64_mingw -O C:\Qt6

cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt6\6.8.3\mingw_64"
cmake --build build --parallel

# Dołącz biblioteki Qt obok exe (wymagane do uruchomienia na innych PC)
$env:PATH = "C:\Qt6\6.8.3\mingw_64\bin;" + $env:PATH
windeployqt6 build\usb_fat32_creator.exe --no-translations
```

Aplikacja jest zbudowana z manifestem UAC — przy uruchomieniu automatycznie poprosi o uprawnienia administratora.

## Portable EXE (Windows)

### Opcja A: automatycznie przez GitHub Actions

Workflow: `.github/workflows/windows-portable.yml`

Co robi pipeline:
- buduje wersję `Release`,
- uruchamia `windeployqt` (dołącza zależności Qt),
- pakuje aplikację do ZIP,
- publikuje artifact `usb-fat32-creator-windows-portable`.

Dla tagów `v*` (np. `v1.0.2`) ten sam ZIP trafia także do GitHub Releases.

## Gotowe ZIP-y dla macOS i Linux

Workflow: `.github/workflows/unix-zips.yml`

Pipeline buduje i publikuje:
- `usb-fat32-creator-macos-portable` (artifact ZIP z aplikacją `.app`),
- `usb-fat32-creator-linux-portable` (artifact ZIP z binarką i skryptem startowym).

Dla tagów `v*` oba ZIP-y trafiają także do GitHub Releases jako assety.

### Jak pobrać gotowe paczki

1. Wejdź w zakładkę Actions.
2. Uruchom workflow `unix-zips-build`.
3. Po zakończeniu pobierz artifacty:
	- `usb-fat32-creator-macos-portable`,
	- `usb-fat32-creator-linux-portable`.

### Uwaga o Linux

Paczka Linux zawiera gotową binarkę i skrypt uruchomieniowy, ale na części dystrybucji może wymagać obecnego systemowego runtime Qt6.

### Opcja B: ręcznie na Windows

Po zbudowaniu `Release` dołącz zależności Qt:

```powershell
$exe = ".\build\Release\usb_fat32_creator.exe"
windeployqt --release --compiler-runtime --no-translations --no-system-d3d-compiler --no-opengl-sw $exe
```

Następnie spakuj katalog z EXE i bibliotekami do ZIP.

## Jak działa aplikacja

1. Wybierasz fizyczne urządzenie.
2. Ustawiasz liczbę partycji FAT32 (lub auto).
3. Klikasz `Testuj`, aby zobaczyć symulację i przejść walidację.
4. Klikasz `Zbuduj plan`, aby wygenerować komendy.
5. Klikasz `Wykonaj` tylko po świadomej akceptacji ryzyka.

## Ograniczenia i uwagi

- Aplikacja jest multiplatformowa, ale binarki budujesz natywnie na danym systemie.
- Na Windows ścieżka wykonania używa `diskpart` — aplikacja automatycznie generuje skrypt tymczasowy i go wykonuje.
- Zawsze testuj na nośniku nieprodukcyjnym, zanim użyjesz narzędzia operacyjnie.
