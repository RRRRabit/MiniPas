program t18
type
  vec = array[0..3] of real;
var
  v: vec;
  i: integer;
  t: real;
begin
  i := 0;
  t := 0.0;
  while i < 4 do
  begin
    v[i] := i + 0.5;
    t := t + v[i];
    i := i + 1
  end
end.
