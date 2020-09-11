# pixel_search
ahk pixel search functions


## TEMP BIN

if (CurrentKeyPressed == Vars.m1button)
{
                        var Frame = GlobalFrame;
                        NewFrame = false;

                        if (Frame.Length != 0)
                        {
                            Point[] array2 = (from t in Frame orderby t.Y select t).ToArray<Point>();
                            List<Vector2> list = new List<Vector2>();

                            for (int j = 0; j < array2.Length; j++)
                            {
                                Vector2 current = new Vector2((float)array2[j].X, (float)array2[j].Y);
                                if (!(from t in list where (t - current).Length() < Vars.size || Math.Abs(t.X - current.X) < Vars.size select t).Any())
                                {
                                    list.Add(current);
                                    if (list.Count > 0)
                                    {
                                        break;
                                    }
                                }
                            }

                            Vector2 vector = (from t in list select t - new Vector2((float)(Vars.xSize / 2), (float)(Vars.ySize / 2)) into t orderby t.Length() select t).ElementAt(0) + new Vector2(1f, (float)Vars.closeSize);
                            if (Vars.RCS == false)
                            {
                                com.Move((int)(vector.X * Vars.speedSpray), (int)(vector.Y * Vars.speedSpray / Vars.slowYSpray), false);
                            }
                            else
                            {

                                if (rcs_counter == 0 && System.DateTime.Now.Subtract(lastshot).TotalMilliseconds < Vars.msBetweenRCS + Utility.GetRandomInt(0, Vars.RandomDelay))
                                {
                                    com.Move((int)(vector.X * Vars.speedSpray), (int)(vector.Y * Vars.speedSpray), false);
                                }
                                else if (rcs_counter == 0) ++rcs_counter;

                                if (rcs_counter == 1)
                                {
                                    com.Move((int)(vector.X * Vars.speedSpray), (int)(vector.Y * Vars.speedSpray), false);
                                    lastshot = System.DateTime.Now;
                                    ++rcs_counter;
                                }
                                else if (rcs_counter == 2)
                                {
                                    if (System.DateTime.Now.Subtract(lastshot).TotalMilliseconds < Vars.rcs_strength)
                                    {
                                        com.Move((int)(vector.X * Vars.speedSpray), (int)(1), false);
                                    }
                                    else ++rcs_counter;
                                }
                                else if (rcs_counter == 3)
                                {
                                    com.Move((int)(vector.X * Vars.speedSpray), (int)(0), false);
                                }

                            }
                        }

                    }
                    // Single Fire
                    else if (CurrentKeyPressed == Vars.singleFireKey)
                    {
                        var Frame = GlobalFrame;
                        NewFrame = false;

                        if (Frame.Length != 0)
                        { // IF NOT ERROR

                            var q = Frame.OrderBy(t => t.Y).ToArray();

                            List<Vector2> forbidden = new List<Vector2>();

                            for (int i = 0; i < q.Length; i++)
                            {
                                Vector2 current = new Vector2(q[i].X, q[i].Y);
                                if (forbidden.Where(t => (t - current).Length() < Vars.size || Math.Abs(t.X - current.X) < Vars.size).Count() < 1)
                                { // TO NOT PLACE POINTS AT THE BODY
                                    forbidden.Add(current);
                                    if (forbidden.Count > Vars.maxCounts)
                                    {
                                        break;
                                    }
                                }
                            }

                            // SHOOTING
                            bool pressDown = false;
                            var closes = forbidden.Select(t => (t - new Vector2(Vars.xSize / 2, Vars.ySize / 2))).OrderBy(t => t.Length()).ElementAt(0) + new Vector2(1, 1);
                            if (closes.Length() < (Vars.closeSize))
                            {
                                if (Vars.canShoot)
                                {
                                    if (System.DateTime.Now.Subtract(lastshot).TotalMilliseconds > Vars.msBetweenShots + Utility.GetRandomInt(0, Vars.RandomDelay))
                                    {
                                        lastshot = System.DateTime.Now;
                                        pressDown = true;
                                    }
                                }
                            }

                            if (!first_delay && !first_action)
                            {
                                firstshot = System.DateTime.Now;
                                first_delay = true;
                            }
                            else if (!first_action && System.DateTime.Now.Subtract(firstshot).TotalMilliseconds > 64)
                            {
                                first_action = true;
                            }

                            if (first_action)
                            {
                                com.Move((int)(closes.X * Vars.speed), (int)(closes.Y * Vars.speed), pressDown);
                            }

                        }


                    }    
                    }
