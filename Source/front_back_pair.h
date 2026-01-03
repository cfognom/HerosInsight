#pragma once

namespace HerosInsight
{
    template <typename T>
    struct FrontBackPair
    {
        T pair[2]{};
        bool state = false;

        T &front() { return pair[state]; }
        const T &front() const { return pair[state]; }

        T &back() { return pair[!state]; }
        const T &back() const { return pair[!state]; }

        void flip() { state = !state; }
    };
};