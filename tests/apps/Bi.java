
public class Bi
{
  private static int nb_loops = 0;
  private static boolean quiet = false;

  private static int do_one_loop(int i, int n)
  {
    int j;
    for (j = 0; j < 100000000; j++)
      n = n + i * j ;

    return n;
  }

  public static void main(String [] args)
  {
    if (args.length == 2) {
      if (args[0].equals("-q"))
	quiet = true;

      nb_loops = Integer.parseInt(args[1]);

    } else if (args.length == 1) {
      if (args[0].equals("-q"))
	quiet = true;
      else
	nb_loops = Integer.parseInt(args[0]);
    }

    if (!quiet)
      System.out.println("Running bi\n");

    int n = 42;
    for (int i = 0; nb_loops == 0 || i < nb_loops; i++) {
      do_one_loop(i, n);
      if (!quiet)
	System.out.println(i);
    }

    if (!quiet)
      System.out.println("Exiting bi\n");
  }
}