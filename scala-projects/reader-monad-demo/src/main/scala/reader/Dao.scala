package reader

case class Session(db: String)

object Dao {

  type DbOp[T] = Session ⇒ T

  def runDbOps[T](session: Session)(dbOp: DbOp[T]): T = {
    println(s"Running db operation with session [$session]")
    dbOp(session)
  }

  def transactional[T](dbOp: DbOp[T]): DbOp[T] = { session ⇒
    println(s"Begining transaction with session [$session]")
    val res = dbOp(session)
    println(s"Committing transaction with session [$session]")
    res
  }
}

class Dao {

  import Dao._

  val findAllBlogs: DbOp[Seq[Blog]] = { session ⇒
    println(s"find all blogs using session [$session]")
    Seq(
      Blog(1, "Hello World", "Hey there"),
      Blog(2, "Monad rocks", "You should learn monads now!"),
      Blog(3, "Scala is awesome", "Awesome!")
    )
  }

  def findComments(blogId: Int): DbOp[Seq[Comment]] = { session ⇒
    println(s"find comments by blog id: [$blogId] using session [$session]")
    Seq(
      Comment(1, "A comment", "John Doe", blogId),
      Comment(2, "Another comment", "Unknown", blogId)
    )
  }

  val findAllComments: DbOp[Seq[Comment]] = { session ⇒
    println(s"find all comments using session [$session]")
    Seq(
      Comment(1, "A comment", "John Doe", 1),
      Comment(2, "Oh yeah", "Someone", 1),
      Comment(3, "Another comment", "Unknown", 2)
    )
  }

}
