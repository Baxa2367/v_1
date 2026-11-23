#include "ai/tip_renderer.h"

namespace lumina::ai {

std::string TipRenderer::render(const std::string& tmpl, const json& vars) {
    std::string out = tmpl;
    for (auto it = vars.begin(); it != vars.end(); ++it) {
        const std::string token = "{" + it.key() + "}";
        std::string value;
        if (it.value().is_string()) value = it.value().get<std::string>();
        else if (it.value().is_number_integer()) value = std::to_string(it.value().get<long long>());
        else if (it.value().is_number()) value = std::to_string(it.value().get<double>());
        size_t pos = 0;
        while ((pos = out.find(token, pos)) != std::string::npos) {
            out.replace(pos, token.size(), value);
            pos += value.size();
        }
    }
    return out;
}

std::string TipRenderer::fallback(const std::string& kind, const std::string& locale,
                                  const json& vars) {
    const std::string topic = vars.value("topic", "");
    const int streak = vars.value("streak", 0);
    const int accuracy = vars.value("accuracy", 0);

    if (locale == "ru") {
        if (kind == "warning")
            return "Серия ошибок по теме «" + topic + "». Снизим сложность и разберём базовый пример — это нормальная часть обучения.";
        if (kind == "congratulation")
            return "Отличная работа! Точность " + std::to_string(accuracy) + "%, серия " + std::to_string(streak) + " дней. Продолжай в том же темпе.";
        if (kind == "motivational")
            return "До дневной цели осталось немного. Одно задание — и день закрыт.";
        return "Совет: вернись к теме «" + topic + "» через повторение — 10 минут теории перед практикой дают заметный прирост.";
    }
    if (locale == "en") {
        if (kind == "warning")
            return "Several mistakes in \"" + topic + "\". We lowered the difficulty and added a short recap - this is a normal part of learning.";
        if (kind == "congratulation")
            return "Great job! Accuracy " + std::to_string(accuracy) + "%, a " + std::to_string(streak) + "-day streak. Keep going.";
        if (kind == "motivational")
            return "You are close to today's goal. One task and the day is complete.";
        return "Tip: revisit \"" + topic + "\" with spaced repetition - ten minutes of theory before practice pays off.";
    }
    if (kind == "warning")
        return "«" + topic + "» тақырыбында қателер қатары тіркелді. Қиындықты төмендетіп, қысқаша түсіндірме береміз — бұл оқудың қалыпты бөлігі.";
    if (kind == "congratulation")
        return "Керемет! Дәлдік " + std::to_string(accuracy) + "%, " + std::to_string(streak) + " күндік серия. Осы қарқынмен жалғастыр.";
    if (kind == "motivational")
        return "Күндік мақсатқа аз қалды. Бір тапсырма — күн жабылады.";
    return "Кеңес: «" + topic + "» тақырыбын аралық қайталаумен бекіт — практика алдында 10 минут теория айтарлықтай нәтиже береді.";
}

} // namespace lumina::ai
