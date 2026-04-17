# USB FAT32 Creator (C++/Qt)

Okienkowa aplikacja multiplatformowa (macOS/Linux/Windows), która pracuje na **urządzeniach fizycznych** i generuje plan podziału:
- FAT32 o maksymalnym rozmiarze 32 GB na partycję
- reszta nośnika jako exFAT
- opcjonalnie wiele „wirtualnych penów” FAT32 (wiele partycji FAT32)

## Ważne bezpieczeństwo
- Aplikacja domyślnie działa w trybie `dry-run`.
- Operacje partycjonowania kasują dane.
- Wymagane są uprawnienia administratora/root.

## Wymagania
- CMake >= 3.21
- Kompilator C++17
- Qt6 Widgets

## Build (macOS/Linux)
```bash
cmake -S . -B build
cmake --build build -j
./build/usb_fat32_creator
```

## Build (Windows, PowerShell)
```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\usb_fat32_creator.exe
```

## Portable EXE (Windows)

### Opcja A: automatycznie przez GitHub Actions
- Workflow: `.github/workflows/windows-portable.yml`
- Uruchom: **Actions -> windows-portable-build -> Run workflow**
- Wynik: artifact `usb-fat32-creator-windows-portable` zawierający ZIP z `usb_fat32_creator.exe` i bibliotekami Qt.
- Dla tagu `v*` (np. `v1.0.0`) workflow dodatkowo publikuje ZIP jako asset w GitHub Release.

### Opcja B: ręcznie na Windows
Po kompilacji `Release` uruchom `windeployqt`, aby dołączyć zależności Qt:

```powershell
$exe = ".\build\Release\usb_fat32_creator.exe"
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw $exe
```

Następnie spakuj zawartość katalogu z EXE do ZIP i to będzie wersja portable.

## Szybki flow "ma hulać"
1. Zrób push kodu na GitHub.
2. Uruchom workflow `windows-portable-build`.
3. Pobierz artifact ZIP i rozpakuj na Windows.
4. Uruchom `usb_fat32_creator.exe` jako Administrator.
5. Najpierw użyj `Testuj`, potem `Zbuduj plan`, a `Wykonaj` dopiero po potwierdzeniu.

## Jak to działa
1. Wykrywa fizyczne urządzenia dyskowe (nie partycje).
2. Tworzy logiczny plan partycji FAT32/exFAT.
3. Generuje listę poleceń systemowych per platforma.
4. W trybie normalnym wykonuje polecenia sekwencyjnie (Windows: tylko generacja planu diskpart).

## Uwagi multiplatformowe
- Kod aplikacji jest wspólny i przenośny.
- Warstwa operacji dyskowych jest rozdzielona per OS.
- Binarka nie jest „jedna na wszystkie OS”; kompilujesz natywnie na danym systemie i tam działa.
