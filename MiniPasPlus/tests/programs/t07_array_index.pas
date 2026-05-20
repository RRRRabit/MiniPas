program t07
type
  arr = array[1..5] of integer;
var
  a: arr;
  i,s: integer;
begin
  i := 1;
  s := 0;
  while i < 6 do
  begin
    a[i] := i * 2;
    s := s + a[i];
    i := i + 1
  end
end.
