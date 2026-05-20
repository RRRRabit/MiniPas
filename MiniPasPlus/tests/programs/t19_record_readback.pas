program t19
type
  node = record
    v: integer;
    w: real;
  end;
var
  n: node;
  a: integer;
  b: real;
begin
  n.v := 11;
  n.w := 2.5;
  a := n.v;
  b := n.w + 1.0
end.
