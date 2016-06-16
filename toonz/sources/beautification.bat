@echo off

for /R %%F in (*.cpp;*.hpp;*.c;*.h) do (
  clang-format -i "%%F"
)
