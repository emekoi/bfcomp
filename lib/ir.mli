type t =
  |  Tape of int
  | Cell of int
  | Read
  | Write
  | Set of int
  | Loop of t list

val pp : Format.formatter -> t -> unit
val show : t -> string

module Pass : sig
  val finalize : 'a list -> 'a list
  val zero_loop : t list -> t -> t list
  val dead_code : t list -> t -> t list
  val contraction : t list -> t -> t list
  val run : (t list -> t -> t list) list -> t list -> t list
end
