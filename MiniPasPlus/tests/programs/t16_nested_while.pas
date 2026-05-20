program t16
var
  i,j,s: integer;
begin
  i := 1;
  s := 0;
  while i < 4 do
  begin
    j := 1;
    while j < 4 do
    begin
      s := s + i * j;
      j := j + 1
    end;
    i := i + 1
  end
end.
