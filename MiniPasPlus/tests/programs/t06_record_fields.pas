program t06
type
  person = record
    age: integer;
    score: real;
  end;
var
  p: person;
begin
  p.age := 18;
  p.score := 95.5
end.
