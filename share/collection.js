function format_instance(inst)
{
  return ("<li class=\"instance\"><a href=\"" + location.pathname
      + "?action=show_graph;" + inst.params + "\">" + inst.description
      + "</a></li>");
}

function format_instance_list(instances)
{
  var ret = "<ul class=\"instance_list\">";
  var i;

  if (instances.length == 0)
    return ("");

  for (i = 0; i < instances.length; i++)
    ret += format_instance (instances[i]);
  
  ret += "</ul>";

  return (ret);
}

function format_graph(graph)
{
  return ("<li class=\"graph\">" + graph.title + format_instance_list (graph.instances) + "</li>");
}

function update_search_suggestions ()
{
  var term = $("#search-input").val ();
  if (term.length < 2)
  {
    $("#search-suggest").hide ();
    return (true);
  }

  $("#search-suggest").show ();
  $.getJSON ("collection.fcgi",
    { "action": "search_json", "q": term},
    function(data)
    {
      var i;
      $("#search-suggest").html ("");
      for (i = 0; i < data.length; i++)
      {
        var graph = data[i];
        $("#search-suggest").append (format_graph (graph));
      }
    }
  );
} /* update_search_suggestions */

$(document).ready(function() {
    /* $("#layout-middle-right").html ("<ul id=\"search-suggest\" class=\"graph_list\"></ul>"); */
    $("#search-form").append ("<ul id=\"search-suggest\" class=\"graph_list\"></ul>");
    $("#search-suggest").hide ();

    $("#search-input").blur (function()
    {
      $("#search-suggest").hide ();
    });

    $("#search-input").focus (function()
    {
      var term = $("#search-input").val ();
      if (term.length < 2)
      {
        $("#search-suggest").hide ();
      }
      else
      {
        $("#search-suggest").show ();
      }
    });

    $("#search-input").keyup (function()
    {
      update_search_suggestions ();
    });
});

/* vim: set sw=2 sts=2 et fdm=marker : */
