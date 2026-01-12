# Документация NovelMind

> **Навигационная карта документации проекта NovelMind**

## Быстрый старт

- [README.md](../README.md) — Обзор проекта и быстрый старт
- [docs/quick_start_guide.md](quick_start_guide.md) — Пошаговое руководство для начинающих
- [docs/troubleshooting_guide.md](troubleshooting_guide.md) — Решение типичных проблем
- [CONTRIBUTING.md](../CONTRIBUTING.md) — Как внести вклад в проект

## 📘 Спецификация редактора (100% готовность)

**[EDITOR_SPEC_100.md](EDITOR_SPEC_100.md)** — Канонический документ, описывающий:
- Определение продукта и границы ответственности
- E2E пользовательские сценарии
- Функциональные требования по модулям
- Архитектурные контракты GUI ↔ engine_core
- Критерии готовности редактора

## Архитектура

### Общая архитектура
- [architecture_overview.md](architecture_overview.md) — Высокоуровневый обзор системы
- [core_runtime_spec.md](core_runtime_spec.md) — API спецификация ядра runtime

### GUI редактор
- [gui_architecture.md](gui_architecture.md) — Архитектура Qt6 GUI редактора
- [gui-assessment.md](gui-assessment.md) — Детальный анализ 140 функций GUI
- [IMPLEMENTATION_ROADMAP.md](IMPLEMENTATION_ROADMAP.md) — **Source of truth** для статусов реализации

### Взаимодействие компонентов
- [editor_engine_interaction.md](editor_engine_interaction.md) — Протоколы обмена данными редактор ↔ движок
- [animation_integration.md](animation_integration.md) — Интеграция системы анимаций

## Технические спецификации

### Язык сценариев
- [nm_script_specification.md](nm_script_specification.md) — Спецификация NM Script 1.0 (на русском)
- [nm_script_language_reference.md](nm_script_language_reference.md) — **NEW** Complete Language Reference (English)
- [nm_script_type_system.md](nm_script_type_system.md) — **NEW** Detailed Type System Documentation
- [nm_script_vm_architecture.md](nm_script_vm_architecture.md) — **NEW** Bytecode & VM Architecture
- [nm_script_ir_specification.md](nm_script_ir_specification.md) — **NEW** Intermediate Representation (IR) System
- [nm_script_standard_library.md](nm_script_standard_library.md) — **NEW** Built-in Functions & Commands Reference

### Файловые форматы
- [pack_file_format.md](pack_file_format.md) — Формат .nmres pack-файлов
- [pack_security.md](pack_security.md) — Безопасность и шифрование AES-256-GCM
- [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) — Стандартная структура проекта

### Совместимость
- [VERSIONING_AND_COMPATIBILITY.md](VERSIONING_AND_COMPATIBILITY.md) — Политика версионирования

## Разработка

### Стандарты кодирования
- [coding_standards.md](coding_standards.md) — C++20 стандарты и соглашения
- [RAII_COMPLIANCE.md](RAII_COMPLIANCE.md) — Верификация соответствия RAII

### Планы развития
- [roadmap.md](roadmap.md) — Дорожная карта разработки
- [workflow_pipeline.md](workflow_pipeline.md) — Конвейер работы с проектом

---

## Как читать документацию

### Для пользователей редактора
1. Начните с [README.md](../README.md) для общего понимания
2. Изучите [quick_start_guide.md](quick_start_guide.md) для практического старта
3. Используйте [troubleshooting_guide.md](troubleshooting_guide.md) при возникновении проблем

### Для разработчиков движка
1. Изучите [architecture_overview.md](architecture_overview.md) для понимания структуры
2. Прочитайте [core_runtime_spec.md](core_runtime_spec.md) для API интерфейсов
3. Следуйте [coding_standards.md](coding_standards.md) при написании кода

### Для разработчиков GUI
1. Начните с [gui_architecture.md](gui_architecture.md)
2. Проверьте актуальные статусы в [IMPLEMENTATION_ROADMAP.md](IMPLEMENTATION_ROADMAP.md)
3. Изучите контракты в [editor_engine_interaction.md](editor_engine_interaction.md)
4. Обратитесь к [EDITOR_SPEC_100.md](EDITOR_SPEC_100.md) для критериев готовности

---

## Глоссарий ключевых терминов

| Термин | Описание |
|--------|----------|
| **engine_core** | Ядро runtime движка (C++) |
| **editor** | Qt6 визуальный редактор |
| **NM Script** | Язык сценариев NovelMind |
| **VFS** | Virtual File System — виртуальная файловая система |
| **Pack** | .nmres файл с упакованными ресурсами |
| **SceneGraph** | Граф сцены с иерархией объектов |
| **StoryGraph** | Визуальный граф истории (узлы и связи) |
| **Event Bus** | Шина событий для слабой связи компонентов |
| **RAII** | Resource Acquisition Is Initialization — паттерн владения ресурсами |

---

*Последнее обновление: 2026-01-11*
