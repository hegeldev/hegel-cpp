#include <cmath>
#include <hegel/hegel.h>
#include <iostream>

#include "fraction.h"

using namespace hegel::st;

int main() {
    // Generate non-zero integers for fractions
    auto nonzero =
        integers<int>({.min_value = -100, .max_value = 100}).filter([](int x) {
            return x != 0;
        });

    auto num1 = integers<int>({.min_value = -100, .max_value = 100}).generate();
    std::cerr << "num1: " << num1 << std::endl;
    auto den1 = nonzero.generate();
    std::cerr << "den1: " << den1 << std::endl;
    auto num2 = integers<int>({.min_value = -100, .max_value = 100}).generate();
    std::cerr << "num2: " << num2 << std::endl;
    auto den2 = nonzero.generate();
    std::cerr << "den2: " << den2 << std::endl;

    Fraction a(num1, den1);
    Fraction b(num2, den2);
    std::cerr << "a = " << num1 << "/" << den1 << std::endl;
    std::cerr << "b = " << num2 << "/" << den2 << std::endl;

    // Property 1: Fraction should equal itself
    if (a != a) {
        std::cerr << "Fraction not equal to itself: " << a.numerator() << "/"
                  << a.denominator() << std::endl;
        return 1;
    }

    // Property 2: a + b should equal b + a (commutativity)
    Fraction sum1 = a + b;
    Fraction sum2 = b + a;
    if (sum1 != sum2) {
        std::cerr << "Addition not commutative" << std::endl;
        std::cerr << "a = " << a.numerator() << "/" << a.denominator()
                  << std::endl;
        std::cerr << "b = " << b.numerator() << "/" << b.denominator()
                  << std::endl;
        std::cerr << "a+b = " << sum1.numerator() << "/" << sum1.denominator()
                  << std::endl;
        std::cerr << "b+a = " << sum2.numerator() << "/" << sum2.denominator()
                  << std::endl;
        return 1;
    }

    // Property 3: a * b should equal b * a (commutativity)
    Fraction prod1 = a * b;
    Fraction prod2 = b * a;
    if (prod1 != prod2) {
        std::cerr << "Multiplication not commutative" << std::endl;
        std::cerr << "a = " << a.numerator() << "/" << a.denominator()
                  << std::endl;
        std::cerr << "b = " << b.numerator() << "/" << b.denominator()
                  << std::endl;
        std::cerr << "a*b = " << prod1.numerator() << "/" << prod1.denominator()
                  << std::endl;
        std::cerr << "b*a = " << prod2.numerator() << "/" << prod2.denominator()
                  << std::endl;
        return 1;
    }

    // Property 4: Denominator should always be positive after normalization
    if (a.denominator() <= 0) {
        std::cerr << "Denominator not positive: " << a.numerator() << "/"
                  << a.denominator() << std::endl;
        std::cerr << "Input was: " << num1 << "/" << den1 << std::endl;
        return 1;
    }

    // Property 5: The double value should match the original ratio
    double expected = static_cast<double>(num1) / static_cast<double>(den1);
    double actual = a.to_double();
    if (std::abs(expected - actual) > 1e-9) {
        std::cerr << "Double conversion wrong: expected " << expected << " got "
                  << actual << std::endl;
        return 1;
    }

    return 0;
}
