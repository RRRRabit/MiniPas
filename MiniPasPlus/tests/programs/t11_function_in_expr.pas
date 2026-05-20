program t11
function inc2(x: integer): integer;
begin
  inc2 := x + 2
end;
var
  a,b: integer;
begin
  a := 7;
  b := inc2(a) + inc2(1)
end.
