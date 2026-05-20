program t10
function mix(a: integer; b: real): real;
var
  t: real;
begin
  t := a + b;
  mix := t * 2.0
end;
var
  r: real;
begin
  r := mix(3,1.25)
end.
