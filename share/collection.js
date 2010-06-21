function format_instance(inst)
{
  return ("<li class=\"instance\"><a href=\"" + location.pathname + "?action=graph;" + inst.params + "\">" + inst.description + "</a></li>");
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

$(document).ready(function() {
    $("#search-input").keyup (function()
    {
      var term = $("#search-input").val ();
      $.getJSON ("collection.fcgi",
        { "action": "search_json", "q": term},
        function(data)
        {
          var i;
          $("#search-output").html ("");
          for (i = 0; i < data.length; i++)
          {
            var graph = data[i];
            $("#search-output").append (format_graph (graph));
          }
        });
    });
});

/* vim: set sw=2 sts=2 et fdm=marker : */
