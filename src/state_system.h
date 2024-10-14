//
// Created by Jocelyn Baker on 2024-09-30.
//

#ifndef STATE_SYSTEM_H
#define STATE_SYSTEM_H



class StateSystem {
public:
    void init();

    static bool is_advanced();
    static void set_advanced();
    static void set_basic();

    static void increment_points(unsigned int value);
    static void reset_points();
    static unsigned int get_points();
private:
    static bool advanced;
    static unsigned int points;
};



#endif //STATE_SYSTEM_H
