# Спецификация языка NM Script 1.0

## Обзор

NM Script — это предметно-ориентированный язык, разработанный для создания визуальных новелл в движке NovelMind. Он предоставляет чистый и интуитивный синтаксис для определения персонажей, сцен, диалогов, выборов и игровой логики.

## Информация о версии

- **Версия**: 1.0
- **Статус**: Черновик
- **Последнее обновление**: 2025-12-10

## Содержание

1. [Лексическая структура](#лексическая-структура)
2. [Типы данных](#типы-данных)
3. [Объявление персонажей](#объявление-персонажей)
4. [Объявление сцен](#объявление-сцен)
5. [Операторы](#операторы)
6. [Выражения](#выражения)
7. [Управление потоком](#управление-потоком)
8. [Встроенные команды](#встроенные-команды)
9. [Комментарии](#комментарии)
10. [Зарезервированные ключевые слова](#зарезервированные-ключевые-слова)
11. [Грамматика (EBNF)](#грамматика-ebnf)
12. [Семантические правила](#семантические-правила)
13. [Политика совместимости](#политика-совместимости)

---

## 1. Лексическая структура

### Идентификаторы

Идентификаторы должны начинаться с буквы или подчёркивания (_), за которым следует любая комбинация букв, цифр или подчёркиваний.

NMScript поддерживает **Unicode-идентификаторы**, что позволяет использовать:
- Кириллицу (русский, украинский и др.)
- Греческие буквы
- CJK-иероглифы (китайский, японский, корейский)
- Арабские и еврейские буквы
- И другие Unicode-алфавиты

```
identifier ::= [\\p{L}_][\\p{L}\\p{N}_]*
```

Где `\p{L}` — любая буква Unicode, `\p{N}` — любая цифра Unicode.

Допустимые примеры:
```nms
hero           // ASCII
_private       // Начинается с подчёркивания
character1     // С цифрой
MainScene      // CamelCase

// Unicode-идентификаторы:
пролог         // Кириллица
персонаж       // Кириллица
εισαγωγή      // Греческий
導入           // Японский
Алексей_1      // Смешанный
```

**Недопустимые идентификаторы:**
```nms
// 123scene    // Начинается с цифры — ОШИБКА
// my-scene    // Содержит дефис — ОШИБКА
// scene name  // Содержит пробел — ОШИБКА
```

> **Примечание:** При создании узлов в Story Graph система автоматически
> санитизирует невалидные идентификаторы, добавляя подчёркивания и числовые
> суффиксы.

### Строковые литералы

Строковые литералы заключаются в двойные кавычки. Поддерживаются escape-последовательности:

- `\n` - новая строка
- `\t` - табуляция
- `\\` - обратный слеш
- `\"` - двойная кавычка
- `\{` - литеральная открывающая фигурная скобка (экранирование команды форматированного текста)

```nms
"Hello, World!"
"Line 1\nLine 2"
"He said \"Hello\""
```

### Числовые литералы

Числа могут быть целыми или с плавающей точкой:

```nms
42          // целое число
3.14        // число с плавающей точкой
-17         // отрицательное целое число
0.5         // десятичная дробь меньше 1
```

### Логические литералы

```nms
true
false
```

---

## 2. Типы данных

NM Script поддерживает следующие типы данных:

| Тип | Описание | Пример |
|------|-------------|---------|
| `string` | Текстовые данные | `"Hello"` |
| `int` | Целые числа | `42` |
| `float` | Числа с плавающей точкой | `3.14` |
| `bool` | Логические значения | `true`, `false` |
| `void` | Отсутствие значения (для команд) | - |

Приведение типов автоматическое в большинстве случаев:
- `int` в `float`: неявное
- `float` в `int`: неявное (усекается)
- Любой в `string`: неявное в строковых контекстах
- Любой в `bool`: 0, 0.0, "", и false являются ложными; всё остальное истинно

---

## 3. Объявление персонажей

Персонажи должны быть объявлены перед использованием. Объявление определяет идентификатор персонажа, отображаемое имя и необязательное оформление.

### Синтаксис

```nms
character <id>(<properties>)
```

### Свойства

| Свойство | Тип | Обязательно | Описание |
|----------|------|----------|-------------|
| `name` | string | Да | Отображаемое имя в диалоге |
| `color` | string | Нет | Цвет имени (hex: `"#RRGGBB"`) |
| `voice` | string | Нет | Идентификатор озвучки |

### Примеры

```nms
// Базовый персонаж
character Hero(name="Alex")

// Персонаж с оформлением
character Villain(name="Lord Darkness", color="#FF0000")

// Персонаж с озвучкой
character Narrator(name="Narrator", voice="narrator_voice")
```

---

## 4. Объявление сцен

Сцены являются основной организационной единицей в NM Script. Каждая сцена содержит последовательность операторов.

### Синтаксис

```nms
scene <id> {
    <statements>
}
```

### Правила

- Идентификаторы сцен должны быть уникальными в пределах скрипта
- Должна быть определена хотя бы одна сцена
- Сцены могут ссылаться на другие сцены через `goto`

### Пример

```nms
scene intro {
    show background "bg_city_night"
    show Hero at center

    say Hero "Welcome to the adventure!"

    goto chapter1
}

scene chapter1 {
    // Содержимое главы 1
}
```

---

## 5. Операторы

### Оператор Show

Отображает визуальные элементы на экране.

```nms
// Показать фон
show background "<texture_id>"

// Показать персонажа
show <character_id> at <position>
show <character_id> at <position> with <expression>

// Позиции: left, center, right, или пользовательские координаты
show Hero at left
show Hero at center with "happy"
show Hero at (100, 200)
```

### Оператор Hide

Удаляет визуальные элементы с экрана.

```nms
hide <character_id>
hide background
```

### Оператор Say

Отображает текст диалога.

```nms
say <character_id> "<text>"

// Примеры
say Hero "Hello!"
say Narrator "The story begins..."
```

Команды форматированного текста могут быть встроены в диалоги:

| Команда | Описание | Пример |
|---------|-------------|---------|
| `{w=N}` | Ждать N секунд | `"Hello...{w=0.5}world"` |
| `{speed=N}` | Установить скорость набора | `{speed=50}` |
| `{color=#RRGGBB}` | Изменить цвет текста | `{color=#FF0000}Red` |
| `{/color}` | Сбросить цвет | `{/color}` |
| `{shake}` | Эффект тряски | `{shake}Scary!{/shake}` |
| `{wave}` | Эффект волны | `{wave}Hello~{/wave}` |

### Оператор Choice

Представляет варианты выбора игроку.

```nms
choice {
    "Option 1" -> <action>
    "Option 2" -> <action>
    "Option 3" if <condition> -> <action>
}
```

Действия могут быть:
- `goto <scene_id>` - Переход к сцене
- Блок операторов `{ ... }`

### Пример

```nms
say Hero "What should I do?"

choice {
    "Go left" -> goto left_path
    "Go right" -> goto right_path
    "Wait" -> {
        say Hero "I'll wait here..."
        goto wait_scene
    }
    "Secret option" if has_key -> goto secret_path
}
```

### Оператор Set

Присваивает значения переменным или флагам.

```nms
// Установить переменную
set <variable> = <expression>

// Установить флаг
set flag <flag_name> = <bool>

// Примеры
set points = 10
set player_name = "Alex"
set flag visited_shop = true
```

### Оператор Wait

Приостанавливает выполнение.

```nms
wait <seconds>

// Пример
wait 1.5
```

### Оператор Goto

Переходит к другой сцене.

```nms
goto <scene_id>
```

### Оператор Transition

Применяет визуальный переход.

```nms
transition <type> <duration>

// Типы: fade, dissolve, slide_left, slide_right, slide_up, slide_down
transition fade 0.5
transition dissolve 1.0
```

### Оператор Play

Воспроизводит аудио (музыка/звуки).

```nms
// Воспроизвести музыку (по умолчанию зацикленная)
play music "<track_id>"
play music "<track_id>" loop=false

// Воспроизвести звуковой эффект
play sound "<sound_id>"
```

### Оператор Stop

Останавливает музыку.

```nms
stop music
stop music fade=1.0
```

---

## 6. Выражения

### Операторы

#### Арифметические

| Оператор | Описание | Пример |
|----------|-------------|---------|
| `+` | Сложение | `a + b` |
| `-` | Вычитание | `a - b` |
| `*` | Умножение | `a * b` |
| `/` | Деление | `a / b` |
| `%` | Остаток от деления | `a % b` |

#### Сравнения

| Оператор | Описание | Пример |
|----------|-------------|---------|
| `==` | Равно | `a == b` |
| `!=` | Не равно | `a != b` |
| `<` | Меньше | `a < b` |
| `<=` | Меньше или равно | `a <= b` |
| `>` | Больше | `a > b` |
| `>=` | Больше или равно | `a >= b` |

#### Логические

| Оператор | Описание | Пример |
|----------|-------------|---------|
| `&&` | Логическое И | `a && b` |
| `\|\|` | Логическое ИЛИ | `a \|\| b` |
| `!` | Логическое НЕ | `!a` |

### Приоритет (от высшего к низшему)

1. `!` (унарное НЕ)
2. `*`, `/`, `%`
3. `+`, `-`
4. `<`, `<=`, `>`, `>=`
5. `==`, `!=`
6. `&&`
7. `||`

### Примеры

```nms
set total = price * quantity
set can_afford = money >= total
set should_buy = can_afford && wants_item
```

---

## 7. Управление потоком

### Оператор If

Условное выполнение.

```nms
if <condition> {
    <statements>
}

if <condition> {
    <statements>
} else {
    <statements>
}

if <condition> {
    <statements>
} else if <condition> {
    <statements>
} else {
    <statements>
}
```

### Пример

```nms
if points >= 100 {
    say Narrator "You've earned the best ending!"
    goto best_ending
} else if points >= 50 {
    say Narrator "You've done well."
    goto good_ending
} else {
    say Narrator "Better luck next time..."
    goto bad_ending
}
```

### Проверка флагов

```nms
if flag visited_shop {
    say Shopkeeper "Welcome back!"
}

if !flag has_key {
    say Hero "The door is locked..."
}
```

---

## 8. Встроенные команды

На текущем этапе реализации доступны базовые команды ожидания, переходов и
аудио. Расширенные эффекты/анимации UI запланированы, но не включены в VM.

---

## 9. Комментарии

```nms
// Однострочный комментарий

/*
 * Многострочный
 * комментарий
 */

character Hero(name="Alex")  // Встроенный комментарий
```

---

## 10. Зарезервированные ключевые слова

Следующие идентификаторы зарезервированы и не могут использоваться в качестве имён переменных, персонажей или сцен:

```
and         character   choice      else        false
flag        goto        hide        if          music
not         or          play        say         scene
set         show        sound       stop        then
transition  true        wait        with
```

---

## 11. Грамматика (EBNF)

```ebnf
program         = { character_decl | scene_decl } ;

character_decl  = "character" identifier "(" property_list ")" ;
property_list   = [ property { "," property } ] ;
property        = identifier "=" expression ;

scene_decl      = "scene" identifier "{" { statement } "}" ;

statement       = show_stmt
                | hide_stmt
                | say_stmt
                | choice_stmt
                | if_stmt
                | set_stmt
                | goto_stmt
                | wait_stmt
                | transition_stmt
                | play_stmt
                | stop_stmt
                | block_stmt ;

show_stmt       = "show" ("background" string | identifier ["at" position] ["with" string]) ;
hide_stmt       = "hide" (identifier | "background") ;
say_stmt        = "say" identifier string ;
choice_stmt     = "choice" "{" { choice_option } "}" ;
choice_option   = string ["if" expression] "->" (goto_stmt | block_stmt) ;
if_stmt         = "if" expression block_stmt { "else" "if" expression block_stmt } [ "else" block_stmt ] ;
set_stmt        = "set" ["flag"] identifier "=" expression ;
goto_stmt       = "goto" identifier ;
wait_stmt       = "wait" number ;
transition_stmt = "transition" identifier number ;
play_stmt       = "play" ("music" | "sound") string [ play_options ] ;
play_options    = { identifier "=" expression } ;
stop_stmt       = "stop" ("music") [ "fade" "=" number ] ;
block_stmt      = "{" { statement } "}" ;

position        = "left" | "center" | "right" | "(" number "," number ")" ;

expression      = or_expr ;
or_expr         = and_expr { "||" and_expr } ;
and_expr        = equality_expr { "&&" equality_expr } ;
equality_expr   = comparison_expr { ("==" | "!=") comparison_expr } ;
comparison_expr = additive_expr { ("<" | "<=" | ">" | ">=") additive_expr } ;
additive_expr   = multiplicative_expr { ("+" | "-") multiplicative_expr } ;
multiplicative_expr = unary_expr { ("*" | "/" | "%") unary_expr } ;
unary_expr      = ["!" | "-"] primary_expr ;
primary_expr    = number | string | "true" | "false" | identifier | "flag" identifier | "(" expression ")" ;

identifier      = letter { letter | digit | "_" } ;
number          = digit { digit } [ "." digit { digit } ] ;
string          = '"' { character } '"' ;
```

---

## 12. Семантические правила

### Правила для персонажей

1. Персонажи должны быть объявлены перед использованием в операторах `say` или `show`
2. Идентификаторы персонажей должны быть уникальными
3. Имена персонажей могут содержать любые символы UTF-8

### Правила для сцен

1. Идентификаторы сцен должны быть уникальными
2. Должна быть определена хотя бы одна сцена
3. Все цели `goto` должны ссылаться на существующие сцены
4. Пустые сцены генерируют предупреждение

### Правила для переменных

1. Переменные динамически создаются при первом присваивании
2. Имена переменных следуют правилам идентификаторов
3. Переменные имеют глобальную область видимости в пределах скрипта

### Правила для флагов

1. Флаги — это переменные только логического типа
2. К флагам обращаются с префиксом `flag` в условиях
3. Флаги по умолчанию имеют значение `false`, если не установлены

### Правила для выборов

1. Выборы должны иметь хотя бы один вариант
2. Каждый вариант должен иметь пункт назначения (`goto` или блок)
3. Условные варианты используют синтаксис `if`

---

## 13. Политика совместимости

### Версионирование

NM Script использует семантическое версионирование:
- **Мажорная** (1.x.x): Ломающие изменения
- **Минорная** (x.1.x): Новые возможности, обратная совместимость
- **Патч** (x.x.1): Исправления ошибок

### Прямая совместимость

Скрипты, написанные для NM Script 1.0, будут работать с:
- Всеми версиями 1.x
- Более высокие версии могут требовать инструменты миграции

### Политика устаревания

1. Функции помечаются как устаревшие в минорных версиях
2. Устаревшие функции генерируют предупреждения компилятора
3. Устаревшие функции удаляются в следующей мажорной версии

### Путь миграции

При возникновении ломающих изменений:
1. Руководство по миграции предоставляется в примечаниях к выпуску
2. Доступен автоматизированный инструмент миграции
3. Льготный период не менее одной мажорной версии

---

## Приложение A: Полный пример

```nms
// Определение персонажей
character Hero(name="Alex", color="#00AAFF")
character Sage(name="Elder Sage", color="#FFD700")
character Narrator(name="", color="#AAAAAA")

// Вступительная сцена
scene intro {
    transition fade 1.0
    show background "bg_forest_dawn"

    wait 1.0

    say Narrator "The morning sun broke through the ancient trees..."

    show Hero at center

    say Hero "Today is the day. I must find the Elder Sage."

    play music "exploration_theme"

    choice {
        "Head north" -> goto forest_path
        "Check inventory" -> {
            say Hero "Let me see what I have..."
            set flag checked_inventory = true
            goto intro
        }
        "Wait for a sign" -> goto wait_scene
    }
}

scene forest_path {
    show background "bg_forest_deep"
    transition dissolve 0.5

    say Narrator "The path grew darker as Alex ventured deeper."

    if flag checked_inventory {
        say Hero "Good thing I checked my supplies."
        set preparation_bonus = 10
    }

    move Hero to left duration=1.0

    show Sage at right with "mysterious"

    say Sage "I have been expecting you, young one."

    set points = points + 50

    goto sage_dialogue
}

scene wait_scene {
    wait 2.0

    say Narrator "Nothing happened. Perhaps action is needed."

    goto intro
}

scene sage_dialogue {
    say Sage "The path ahead is treacherous."
    say Sage "But I sense great potential in you."

    say Hero "What must I do?"

    say Sage "First, you must prove your worth."

    choice {
        "I am ready." -> {
            set flag accepted_quest = true
            set points = points + 25
            goto trial_begin
        }
        "I need more time." -> {
            say Sage "Time... is a luxury we may not have."
            goto sage_dialogue
        }
    }
}

scene trial_begin {
    play music "epic_theme" loop=true
    transition fade 0.3

    say Narrator "And so, the trial began..."

    // Содержимое испытания продолжается здесь
}
```

---

## Приложение B: Коды ошибок

| Код | Серьёзность | Описание |
|------|----------|-------------|
| E3001 | Ошибка | Неопределённый персонаж |
| E3002 | Ошибка | Дублирование определения персонажа |
| E3003 | Предупреждение | Неиспользуемый персонаж |
| E3101 | Ошибка | Неопределённая сцена |
| E3102 | Ошибка | Дублирование определения сцены |
| E3103 | Предупреждение | Неиспользуемая сцена |
| E3104 | Предупреждение | Пустая сцена |
| E3105 | Предупреждение | Недостижимая сцена |
| E3201 | Ошибка | Неопределённая переменная |
| E3202 | Предупреждение | Неиспользуемая переменная |
| E3301 | Предупреждение | Обнаружен недостижимый код |
| E3601 | Ошибка | Пустой блок выбора |

---

*Конец спецификации NM Script 1.0*
