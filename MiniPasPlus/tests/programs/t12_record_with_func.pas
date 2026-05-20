program t12
type
  pair = record
    x: integer;
    y: integer;
  end;
function sum2(a,b: integer): integer;
begin
  sum2 := a + b
end;
var
  p: pair;
  z: integer;
begin
  p.x := 6;
  p.y := 9;
  z := sum2(p.x,p.y)
end.
