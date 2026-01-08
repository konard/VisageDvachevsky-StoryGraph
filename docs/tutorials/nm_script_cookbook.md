# Поваренная книга NM Script

Коллекция готовых решений и паттернов для создания визуальных новелл.

## Содержание

- [Диалоги и персонажи](#диалоги-и-персонажи)
- [Выборы и ветвление](#выборы-и-ветвление)
- [Переменные и флаги](#переменные-и-флаги)
- [Переходы и эффекты](#переходы-и-эффекты)
- [Музыка и звук](#музыка-и-звук)
- [Локализация](#локализация)
- [Продвинутые техники](#продвинутые-техники)

---

## Диалоги и персонажи

### Базовая беседа двух персонажей

```nms
character Alice(name="Алиса", color="#4A90D9")
character Bob(name="Боб", color="#D94A4A")

scene dialogue {
    show background "bg_cafe"

    show Alice at left
    show Bob at right

    say Alice "Как дела?"
    say Bob "Отлично, спасибо!"
    say Alice "Рада слышать!"
}
```

### Разговор с несколькими персонажами

```nms
character Alice(name="Алиса", color="#4A90D9")
character Bob(name="Боб", color="#D94A4A")
character Charlie(name="Чарли", color="#4AD94A")

scene group_talk {
    show background "bg_room"

    show Alice at left
    show Bob at center
    show Charlie at right

    say Alice "Собрались все?"
    say Bob "Да, я здесь."
    say Charlie "И я тоже!"
    say Alice "Отлично! Начнём обсуждение."
}
```

### Повествователь и мысли персонажа

```nms
character Hero(name="Алекс", color="#4A90D9")
character Narrator(name="", color="#FFFFFF")  // Пустое имя для повествователя

scene narration {
    show background "bg_forest"

    say Narrator "Тёмный лес окружал путника со всех сторон..."

    show Hero at center

    say Hero "(Мне не следовало сюда приходить...)"  // Скобки для мыслей
    say Narrator "Но пути назад уже не было."
}
```

### Смена эмоций персонажа

```nms
character Alice(name="Алиса", color="#4A90D9")

scene emotions {
    show background "bg_room"

    // Нейтральная
    show Alice at center

    say Alice "Привет!"

    // Счастливая (смена спрайта)
    show Alice "alice_happy" at center

    say Alice "Это так здорово!"

    // Грустная
    show Alice "alice_sad" at center

    say Alice "Но потом всё испортилось..."
}
```

---

## Выборы и ветвление

### Простой выбор с двумя вариантами

```nms
character Hero(name="Герой", color="#4A90D9")

scene decision {
    show background "bg_crossroads"
    show Hero at center

    say Hero "Куда мне пойти?"

    choice {
        "Налево" -> left_path
        "Направо" -> right_path
    }
}

scene left_path {
    say Hero "Я выбрал левый путь."
    goto continue_story
}

scene right_path {
    say Hero "Я выбрал правый путь."
    goto continue_story
}

scene continue_story {
    say Hero "История продолжается..."
}
```

### Выбор с изменением переменных

```nms
character Hero(name="Герой", color="#4A90D9")

var kindness = 0
var courage = 0

scene moral_choice {
    show background "bg_village"
    show Hero at center

    say Hero "Вижу человека в беде. Что делать?"

    choice {
        "Помочь" -> help
        "Пройти мимо" -> ignore
    }
}

scene help {
    set kindness = kindness + 10

    say Hero "Я должен помочь!"
    say "Вы помогли путнику."

    goto aftermath
}

scene ignore {
    say Hero "У меня нет времени..."
    say "Вы прошли мимо."

    goto aftermath
}

scene aftermath {
    if kindness >= 10 {
        say "Ваша доброта будет вознаграждена."
    }
}
```

### Условный выбор (доступен при условии)

```nms
character Hero(name="Герой", color="#4A90D9")

var has_key = false
var strength = 50

scene locked_door {
    show background "bg_door"
    show Hero at center

    say Hero "Дверь заперта. Как её открыть?"

    choice {
        "Использовать ключ" if has_key -> use_key
        "Выбить дверь" if strength >= 70 -> break_door
        "Взломать замок" -> pick_lock
        "Уйти" -> leave
    }
}

scene use_key {
    say Hero "У меня есть ключ!"
}

scene break_door {
    say Hero "Я выбью эту дверь!"
}

scene pick_lock {
    say Hero "Попробую взломать замок..."
}

scene leave {
    say Hero "Может, мне не сюда..."
}
```

### Таймер на выбор

```nms
character Hero(name="Герой", color="#4A90D9")

scene timed_choice {
    show background "bg_danger"
    show Hero at center

    say Hero "Бомба! У меня есть 5 секунд!"

    choice timeout=5.0 default=explode {
        "Красный провод" -> cut_red
        "Синий провод" -> cut_blue
    }
}

scene cut_red {
    say "Вы обрезали красный провод. БАХ!"
}

scene cut_blue {
    say "Вы обрезали синий провод. Таймер остановился!"
}

scene explode {
    say "Время вышло! БАХ!"
}
```

---

## Переменные и флаги

### Система очков отношений

```nms
character Alice(name="Алиса", color="#4A90D9")
character Bob(name="Боб", color="#D94A4A")

var alice_affection = 0
var bob_affection = 0

scene affection_system {
    show background "bg_school"

    say "С кем поговорить?"

    choice {
        "С Алисой" -> talk_alice
        "С Бобом" -> talk_bob
    }
}

scene talk_alice {
    set alice_affection = alice_affection + 5

    show Alice at center
    say Alice "Рада тебя видеть!"

    if alice_affection >= 20 {
        say Alice "Ты мне очень нравишься!"
    }
}

scene talk_bob {
    set bob_affection = bob_affection + 5

    show Bob at center
    say Bob "Привет, приятель!"

    if bob_affection >= 20 {
        say Bob "Ты мой лучший друг!"
    }
}
```

### Инвентарь и предметы

```nms
character Hero(name="Герой", color="#4A90D9")

var has_sword = false
var has_shield = false
var has_potion = false

scene treasure_room {
    show background "bg_treasure"
    show Hero at center

    say Hero "Вижу три предмета. Могу взять только один!"

    choice {
        "Взять меч" -> take_sword
        "Взять щит" -> take_shield
        "Взять зелье" -> take_potion
    }
}

scene take_sword {
    set has_sword = true
    say Hero "Взял меч!"
    goto boss_fight
}

scene take_shield {
    set has_shield = true
    say Hero "Взял щит!"
    goto boss_fight
}

scene take_potion {
    set has_potion = true
    say Hero "Взял зелье!"
    goto boss_fight
}

scene boss_fight {
    show background "bg_boss"

    if has_sword {
        say "Мечом вы побеждаете босса!"
    } else if has_shield {
        say "Щит защищает вас, но победа даётся нелегко."
    } else {
        say "Зелье исцеляет вас, но битва очень тяжёлая."
    }
}
```

### Счётчик событий и множественные концовки

```nms
character Hero(name="Герой", color="#4A90D9")

var good_deeds = 0
var bad_deeds = 0

scene deed_counter {
    // ... различные сцены, где изменяются good_deeds и bad_deeds

    goto ending
}

scene ending {
    show background "bg_ending"
    show Hero at center

    if good_deeds >= 10 && bad_deeds == 0 {
        say "ИДЕАЛЬНАЯ КОНЦОВКА"
        say Hero "Я сделал всё правильно!"
    } else if good_deeds > bad_deeds {
        say "ХОРОШАЯ КОНЦОВКА"
        say Hero "Я старался быть хорошим."
    } else if bad_deeds > good_deeds {
        say "ПЛОХАЯ КОНЦОВКА"
        say Hero "Возможно, я был слишком жесток..."
    } else {
        say "НЕЙТРАЛЬНАЯ КОНЦОВКА"
        say Hero "Я просто делал, что нужно."
    }
}
```

---

## Переходы и эффекты

### Затухание (Fade)

```nms
scene fade_example {
    // Затухание в чёрный экран
    transition fade 1.5

    show background "bg_room"

    // Персонаж появляется с затуханием
    show Hero at center with fade

    say Hero "Я появился!"
}
```

### Слайды (Slides)

```nms
scene slide_example {
    show background "bg_room"

    // Слайд слева
    show Alice at left with slide_left

    say Alice "Я вошла слева!"

    // Слайд справа
    show Bob at right with slide_right

    say Bob "А я справа!"
}
```

### Перемещение персонажей

```nms
character Alice(name="Алиса", color="#4A90D9")
character Bob(name="Боб", color="#D94A4A")

scene movement {
    show background "bg_room"

    show Alice at left
    show Bob at right

    say Alice "Подойду ближе..."

    // Перемещение персонажа
    move Alice to center duration=1.0

    say Bob "Привет!"

    // Быстрое перемещение
    move Bob to center duration=0.3
}
```

### Камера и зум

```nms
scene camera_effects {
    show background "bg_landscape"

    // Зум на объект
    camera zoom 1.5 duration=2.0

    wait 1.0

    // Возврат к нормальному виду
    camera zoom 1.0 duration=1.0

    // Панорама
    camera pan x=100 y=50 duration=3.0
}
```

### Тряска экрана

```nms
scene shake_example {
    show background "bg_room"
    show Hero at center

    say Hero "Что это?!"

    // Тряска экрана при землетрясении/взрыве
    camera shake intensity=10 duration=1.0

    play sound "sfx_explosion.wav"
}
```

---

## Музыка и звук

### Фоновая музыка

```nms
scene music_example {
    // Запуск музыки с плавным нарастанием
    play music "music_theme.wav" loop=true fade_in=2.0

    show background "bg_room"

    say "Музыка начала играть..."

    // Остановка музыки с затуханием
    stop music fade_out=1.5
}
```

### Звуковые эффекты

```nms
scene sound_effects {
    show background "bg_door"

    // Звук двери
    play sound "sfx_door.wav"

    say "Дверь открылась."

    show background "bg_footsteps"

    // Звук шагов
    play sound "sfx_footsteps.wav"

    wait 1.0

    // Звук удара
    play sound "sfx_impact.wav"
}
```

### Голосовая озвучка

```nms
character Alice(name="Алиса", color="#4A90D9")

scene voice_acting {
    show background "bg_room"
    show Alice at center

    // Диалог с голосовой озвучкой
    say Alice "Привет!" voice "voice/alice_001.wav"
    say Alice "Как дела?" voice "voice/alice_002.wav"
}
```

### Смена музыки между сценами

```nms
scene peaceful {
    play music "music_peaceful.wav" loop=true fade_in=1.0

    show background "bg_garden"
    say "Мирная сцена..."

    goto danger
}

scene danger {
    // Плавная смена музыки
    stop music fade_out=0.5
    play music "music_danger.wav" loop=true fade_in=0.5

    show background "bg_dark_forest"
    say "Опасность близко!"
}
```

---

## Локализация

### Использование локализации

```nms
character Hero(name="Герой", color="#4A90D9")

scene localized {
    show background "bg_room"
    show Hero at center

    // Использование ключей локализации
    say Hero loc("dialog.hero.greeting")
    say Hero loc("dialog.hero.question")
}
```

**Файл локализации** `localization/ru.json`:
```json
{
  "dialog.hero.greeting": "Привет, мир!",
  "dialog.hero.question": "Как дела?"
}
```

**Файл локализации** `localization/en.json`:
```json
{
  "dialog.hero.greeting": "Hello, world!",
  "dialog.hero.question": "How are you?"
}
```

### Локализация с параметрами

```nms
var player_name = "Алекс"
var score = 100

scene localized_params {
    // Использование параметров в локализации
    say Hero loc("dialog.score", player_name, score)
}
```

**Файл локализации**:
```json
{
  "dialog.score": "{0}, ваш счёт: {1}!"
}
```

---

## Продвинутые техники

### Визуальная кинематографическая сцена

```nms
scene cutscene {
    // Затухание в чёрный
    transition fade 2.0

    // Драматическая музыка
    play music "music_dramatic.wav" fade_in=2.0

    show background "bg_castle"
    transition fade 2.0

    wait 1.0

    // Зум на замок
    camera zoom 1.5 duration=3.0

    wait 1.0

    show Villain at right with fade

    say Villain "Наконец-то мы встретились!"

    wait 0.5

    show Hero at left with fade

    say Hero "Я не позволю тебе победить!"

    // Тряска перед битвой
    camera shake intensity=5 duration=0.5

    goto battle
}
```

### Система сохранения выборов игрока

```nms
// Глобальные переменные для отслеживания выборов
var choice_chapter1 = ""
var choice_chapter2 = ""
var choice_chapter3 = ""

scene chapter1_choice {
    say "Важный выбор в главе 1!"

    choice {
        "Вариант A" -> {
            set choice_chapter1 = "A"
            goto chapter1_a
        }
        "Вариант B" -> {
            set choice_chapter1 = "B"
            goto chapter1_b
        }
    }
}

scene final_epilogue {
    // Концовка зависит от всех выборов
    if choice_chapter1 == "A" && choice_chapter2 == "A" {
        say "Полностью добрая концовка!"
    } else if choice_chapter1 == "B" && choice_chapter2 == "B" {
        say "Полностью злая концовка!"
    } else {
        say "Смешанная концовка!"
    }
}
```

### Миниигра (простой пример)

```nms
var attempts = 3
var number = 7  // Загаданное число

scene guessing_game {
    say "Загадано число от 1 до 10. Угадайте!"

    choice {
        "5" -> guess_5
        "7" -> guess_7
        "9" -> guess_9
    }
}

scene guess_5 {
    set attempts = attempts - 1

    say "Неправильно! Попробуйте ещё."

    if attempts > 0 {
        goto guessing_game
    } else {
        goto game_over
    }
}

scene guess_7 {
    say "Правильно! Вы угадали!"
    goto win
}

scene guess_9 {
    set attempts = attempts - 1

    say "Неправильно! Попробуйте ещё."

    if attempts > 0 {
        goto guessing_game
    } else {
        goto game_over
    }
}

scene win {
    say "Поздравляем с победой!"
}

scene game_over {
    say "Попытки закончились. Игра окончена."
}
```

### Динамический диалог на основе статистики

```nms
character Hero(name="Герой", color="#4A90D9")
character Mentor(name="Наставник", color="#D9A94A")

var strength = 10
var intelligence = 15
var charisma = 8

scene mentor_evaluation {
    show background "bg_training"
    show Hero at left
    show Mentor at right

    say Mentor "Позволь оценить твой прогресс..."

    // Динамическая оценка на основе статов
    if strength >= 15 {
        say Mentor "Твоя сила впечатляет!"
    } else if strength >= 10 {
        say Mentor "Неплохая сила."
    } else {
        say Mentor "Тебе нужно больше тренироваться."
    }

    if intelligence >= 15 {
        say Mentor "Твой ум остёр, как клинок!"
    }

    if charisma >= 15 {
        say Mentor "У тебя дар убеждения!"
    }

    // Общая оценка
    var total = strength + intelligence + charisma

    if total >= 40 {
        say Mentor "Ты готов к финальному испытанию!"
    } else {
        say Mentor "Продолжай тренировки."
    }
}
```

---

## Рекомендации

### Структура проекта

```
my_project/
├── scripts/
│   ├── main.nms           # Главный файл со всеми персонажами
│   ├── chapter1.nms       # Глава 1
│   ├── chapter2.nms       # Глава 2
│   └── endings.nms        # Все концовки
├── assets/
│   ├── images/
│   │   ├── backgrounds/
│   │   └── characters/
│   ├── audio/
│   │   ├── music/
│   │   ├── sfx/
│   │   └── voice/
│   └── fonts/
└── localization/
    ├── en.json
    └── ru.json
```

### Лучшие практики

1. **Именование**:
   - Используйте понятные имена для сцен: `intro`, `chapter1_choice`, `good_ending`
   - Группируйте связанные сцены префиксами: `ch1_`, `ch2_`

2. **Комментарии**:
   - Комментируйте сложную логику
   - Объясняйте важные выборы игрока

3. **Модульность**:
   - Разделяйте скрипт на файлы по главам
   - Выносите общие переменные в отдельный файл

4. **Тестирование**:
   - Проверяйте все пути выборов
   - Тестируйте граничные случаи переменных
   - Убедитесь, что нет "мёртвых концов" (scenes без выхода)

---

**Следующий шаг**: Изучите [Руководство по устранению неполадок](nm_script_troubleshooting.md) для решения частых проблем.
