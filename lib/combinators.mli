val id : 'a -> 'a
val const : 'a -> 'b -> 'a
val ( $ ) : ('a -> 'b) -> 'a -> 'b
val ( & ) : 'a -> ('a -> 'b) -> 'b
val join : ('a -> 'a -> 'b) -> 'a -> 'b
val flip : ('a -> 'b -> 'c) -> 'b -> 'a -> 'c
val ( << ) : ('a -> 'b) -> ('c -> 'a) -> 'c -> 'b
val ( <*> ) : ('a -> 'b -> 'c) -> ('a -> 'b) -> 'a -> 'c
val ( >*< ) : ('a -> 'b -> 'c) -> ('b -> 'a) -> 'b -> 'c
val converge : ('a -> 'b -> 'c) -> ('d -> 'a) -> ('d -> 'b) -> 'd -> 'c
val on : ('a -> 'a -> 'b) -> ('c -> 'a) -> 'c -> 'c -> 'b
val fix : (('a -> 'b) -> 'a -> 'b) -> 'a -> 'b
