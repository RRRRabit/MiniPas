program t17
function absdiff(a,b: integer): integer;
begin
  if a > b then
    absdiff := a - b
  else
    absdiff := b - a
end;
var
  d: integer;
begin
  d := absdiff(2,9)
end.
