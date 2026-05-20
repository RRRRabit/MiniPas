program t20
function f(a: integer): integer;
begin
  f := a * a
end;
function g(x,y: integer): integer;
var
  t: integer;
begin
  t := f(x) + f(y);
  g := t
end;
var
  ans: integer;
begin
  ans := g(3,4)
end.
