#include <iostream>

struct B {
  B() { std::cout << "B constructed" << std::endl; }
  ~B() { std::cout << "B destructed" << std::endl; }
  virtual void foo() const { std::cout << "B foo" << std::endl; }
};

struct D1 : B {
  D1() { std::cout << "D1 constructed" << std::endl; }
  ~D1() { std::cout << "D1 destructed" << std::endl; }
  void foo() const override { std::cout << "D1 foo" << std::endl; }
};

int main() {
  const B &b = D1();
  b.foo();
}