#include <ruby.h>
#include "extconf.h"

VALUE foobar_foo_initialize(VALUE self)
{
  rb_iv_set(self, "@bar", INT2FIX(42));
  return self;
}

VALUE foobar_foo_bar(VALUE self)
{
  return rb_iv_get(self, "@bar");
}

void Init_foobar()
{
  VALUE mFoobar = rb_define_module("Foobar");
  VALUE cFoo = rb_define_class_under(mFoobar, "Foo", rb_cObject);
  rb_define_method(cFoo, "initialize", foobar_foo_initialize, 0);
  rb_define_method(cFoo, "bar", foobar_foo_bar, 0);
}
